"""
Copyright (C) 2026, KylinSoft Co., Ltd.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
"""
from enum import Enum, IntEnum
import time
import numpy as np
from gesture_actions import AirGestureDBusClient

# 手势阈值常量
AVG_FINGER_OPEN_MIN_DEG = 160.0          # 平均手指伸展角度的最小值
PALM_ANGLE_THRESHOLD_DEG = 40.0          # 掌心法线与摄像头方向的夹角阈值（单位：度）
SWIPE_TIP_DELTA_THRESHOLD = 0.12         # 滑动手势的指尖位移阈值
PALM_AXIS_Y_ANGLE_MIN_DEG = 55.0         # 横向手势时，掌心中轴线与y轴最小夹角（单位：度）
HORIZONTAL_OPEN_MIN_DEG = 135.0          # 左右手势预备态允许的最小平均手指伸展角度
HORIZONTAL_AXIS_Y_ANGLE_MIN_DEG = 32.0   # 左右手势预备态的掌轴与y轴最小夹角（单位：度）
HORIZONTAL_AXIS_PRIORITY_MIN_DEG = 30.0  # 掌轴超过该角度时优先尝试横向识别，避免被纵向误判
HORIZONTAL_PALM_ANGLE_THRESHOLD_DEG = 62.0  # 左右手势预备态的掌心法线角阈值（单位：度）
HORIZONTAL_SWIPE_TIP_DELTA_THRESHOLD = 0.065  # 横向挥手位移阈值
HORIZONTAL_SWIPE_MAX_FRAMES = 22         # 横向挥手滑动窗口帧数
HORIZONTAL_SWIPE_MIN_FRAMES = 5          # 窗口内最少帧数，避免噪声误触
HORIZONTAL_SWIPE_COOLDOWN_FRAMES = 8     # 横向挥手触发后的冷却帧数
HORIZONTAL_LOCK_MIN_FRAMES = 10          # 横向预备态锁定所需连续帧数
VERTICAL_AXIS_Y_ANGLE_MAX_DEG = 25.0     # 竖向手势时，掌心中轴线与y轴最大夹角（单位：度）
BACK_SWIPE_OPEN_MIN_DEG = 145.0          # 手背上挥预备态允许的最小平均手指伸展角度
BACK_SWIPE_PALM_ANGLE_THRESHOLD_DEG = 55.0  # 手背上挥预备态的掌心法线角阈值（单位：度）
BACK_VERTICAL_AXIS_Y_ANGLE_MAX_DEG = 35.0   # 手背上挥预备态允许的掌轴与y轴最大夹角（单位：度）
FINGERTIP_DOWN_MIN_DELTA_Y = 0.02        # 指尖相对MCP平均y最小下移量（用于确认指尖朝下）
FIST_START_OPEN_MIN_DEG = 145.0          # 启动握拳判定所需的最小起始张开度
FIST_CLOSED_MAX_DEG = 125.0              # 握拳时平均手指伸展角度最大值
FIST_OPEN_DROP_MIN_DEG = 38.0            # 握拳时平均手指伸展角度最小下降值
PALM_ACTION_COOLDOWN_FRAMES = 6          # 掌心模式动作触发后的最小冷却帧数
PALM_ACTION_DOMINANCE_RATIO = 1.15       # 下挥/握拳竞争时的优势倍率阈值

# 时间阈值常量
PALM_LOCK_MIN_FRAMES = 15                # 锁定手势状态所需的最小连续帧数
PALM_RELEASE_DELAY_SEC = 2.0             # 从手势丢失到状态重置的延迟时间（单位：秒）
MAX_NO_HAND_FRAMES = 15                  # 连续未检测到手的最大帧数
SWIPE_MAX_FRAMES = 10                    # 滑动手势的最大帧数
FIST_MAX_FRAMES = 15                     # 握拳动作完成的最大帧数

# 手部关键点编号枚举（MediaPipe Hands 21点）
class HandLandmark(IntEnum):
    WRIST = 0
    THUMB_CMC = 1
    THUMB_MCP = 2
    THUMB_IP = 3
    THUMB_TIP = 4
    INDEX_MCP = 5
    INDEX_PIP = 6
    INDEX_DIP = 7
    INDEX_TIP = 8
    MIDDLE_MCP = 9
    MIDDLE_PIP = 10
    MIDDLE_DIP = 11
    MIDDLE_TIP = 12
    RING_MCP = 13
    RING_PIP = 14
    RING_DIP = 15
    RING_TIP = 16
    PINKY_MCP = 17
    PINKY_PIP = 18
    PINKY_DIP = 19
    PINKY_TIP = 20

# 手势状态枚举
class GestureState(Enum):
    PALM_FACING_FINGERTIP_UP = "palm_facing_fingertip_up"
    BACK_FACING_FINGERTIP_DOWN = "back_facing_fingertip_down"
    HORIZONTAL_FINGERTIP_LEFT = "horizontal_fingertip_left"
    HORIZONTAL_FINGERTIP_RIGHT = "horizontal_fingertip_right"
    NONE = "none"


class GestureClassifier:
    def __init__(self):
        self.action_client = AirGestureDBusClient()  # D-Bus客户端对象，执行手势对应操作
        self.locked_state = GestureState.NONE        # 当前锁定的手势状态

        # 每一帧的手部参数
        self.hand_landmarks = []
        self.handedness = None

        # 跟踪状态
        self.no_hand_frames = 0                   # 连续丢手真实帧数
        self.last_hand_frame_index = -1           # 最近一次检测到手的真实帧号（-1 表示最开始的状态）
        self.palm_stable_frames = 0               # 掌心正对稳定帧数
        self.back_stable_frames = 0               # 手背正对稳定帧数
        self.horizontal_left_stable_frames = 0    # 横向左挥预备状态稳定帧数
        self.horizontal_right_stable_frames = 0   # 横向右挥预备状态稳定帧数
        self.avg_finger_open_deg = None           # 平均手指伸展角度
        self.palm_normal = None                   # 掌心法线向量
        self.palm_axis_y_angle_deg = None         # 掌心中轴线与y轴夹角
        self.palm_lost_since = None               # 掌心丢失时间
        self.back_lost_since = None               # 手背丢失时间
        self.horizontal_left_lost_since = None    # 横向左挥预备状态丢失时间
        self.horizontal_right_lost_since = None   # 横向右挥预备状态丢失时间
        self.frame_index = 0                      # 当前帧索引
        self.swipe_start_tip_y = None             # 下挥手起始帧的指尖平均y坐标
        self.swipe_start_frame = None             # 下挥手起始帧索引
        self.fist_start_open_deg = None           # 握拳起始帧的平均手指伸展角度
        self.fist_start_frame = None              # 握拳起始帧索引
        self.palm_action_cooldown_until = 0       # 掌心模式动作冷却截止帧
        self.swipe_up_start_tip_y = None          # 上挥手起始帧的指尖平均y坐标
        self.swipe_up_start_frame = None          # 上挥手起始帧索引
        self.horizontal_swipe_x_history = []    # 横向挥手滑动窗口 x 采样
        self.horizontal_swipe_cooldown_until = 0  # 横向挥手冷却截止帧
        self.reset_tracking_state = 1             # 重置追踪状态

    def classify(self, landmarks, handedness, frame_index):
        self.frame_index = frame_index

        if landmarks is None:
            if self.last_hand_frame_index >= 0:
                # 用真实帧号差计算连续丢手帧数，与跳帧无关
                self.no_hand_frames = frame_index - self.last_hand_frame_index
            else:
                # 这里针对的是第一帧识别为None的情况
                self.no_hand_frames += 1
                # 增加 reset_tracking_state 重置状态判断，避免每次调用
            if self.no_hand_frames >= MAX_NO_HAND_FRAMES and self.reset_tracking_state:
                self._reset_tracking_state()
            return

        self.no_hand_frames = 0
        self.reset_tracking_state = 1
        self.last_hand_frame_index = frame_index
        self.hand_landmarks = landmarks
        self.handedness = handedness
        self._compute_frame_params()
        is_palm_facing_fingertip_up = self._is_palm_facing_fingertip_up()
        is_back_facing_fingertip_down = self._is_back_facing_fingertip_down()
        is_horizontal_fingertip_left = self._is_horizontal_fingertip_left()
        is_horizontal_fingertip_right = self._is_horizontal_fingertip_right()

        if self.locked_state == GestureState.PALM_FACING_FINGERTIP_UP:
            if is_palm_facing_fingertip_up:
                self.palm_lost_since = None
            else:
                now = time.monotonic()
                if self.palm_lost_since is None:
                    self.palm_lost_since = now
                elif now - self.palm_lost_since >= PALM_RELEASE_DELAY_SEC:
                    self.locked_state = GestureState.NONE
                    self.action_client.hide_icon()
                    self._reset_palm_action_candidates()
                    self.palm_stable_frames = 0
                    self.palm_lost_since = None
                    return

            self._update_swipe_down_or_fist_detection()
            return

        if self.locked_state == GestureState.BACK_FACING_FINGERTIP_DOWN:
            if is_back_facing_fingertip_down:
                self.back_lost_since = None
            else:
                now = time.monotonic()
                if self.back_lost_since is None:
                    self.back_lost_since = now
                elif now - self.back_lost_since >= PALM_RELEASE_DELAY_SEC:
                    self.locked_state = GestureState.NONE
                    self.action_client.hide_icon()
                    self.swipe_up_start_tip_y = None
                    self.swipe_up_start_frame = None
                    self.back_stable_frames = 0
                    self.back_lost_since = None
                    return

            self._update_swipe_up_detection()
            return

        if self.locked_state == GestureState.HORIZONTAL_FINGERTIP_LEFT:
            if is_horizontal_fingertip_left:
                self.horizontal_left_lost_since = None
            else:
                now = time.monotonic()
                if self.horizontal_left_lost_since is None:
                    self.horizontal_left_lost_since = now
                elif now - self.horizontal_left_lost_since >= PALM_RELEASE_DELAY_SEC:
                    self.locked_state = GestureState.NONE
                    self.action_client.hide_icon()
                    self._clear_horizontal_swipe_history()
                    self.horizontal_left_stable_frames = 0
                    self.horizontal_left_lost_since = None
                    return

            # 指尖朝左是右挥手预备状态，触发右滑动作
            self._update_swipe_right_detection()
            return

        if self.locked_state == GestureState.HORIZONTAL_FINGERTIP_RIGHT:
            if is_horizontal_fingertip_right:
                self.horizontal_right_lost_since = None
            else:
                now = time.monotonic()
                if self.horizontal_right_lost_since is None:
                    self.horizontal_right_lost_since = now
                elif now - self.horizontal_right_lost_since >= PALM_RELEASE_DELAY_SEC:
                    self.locked_state = GestureState.NONE
                    self.action_client.hide_icon()
                    self._clear_horizontal_swipe_history()
                    self.horizontal_right_stable_frames = 0
                    self.horizontal_right_lost_since = None
                    return

            # 指尖朝右是左挥手预备状态，触发左滑动作
            self._update_swipe_left_detection()
            return

        prefer_horizontal = (
            self.palm_axis_y_angle_deg is not None
            and self.palm_axis_y_angle_deg >= HORIZONTAL_AXIS_PRIORITY_MIN_DEG
        )

        if prefer_horizontal:
            if self._try_lock_horizontal(is_horizontal_fingertip_left, is_horizontal_fingertip_right):
                return
            if self._try_lock_vertical(is_palm_facing_fingertip_up, is_back_facing_fingertip_down):
                return
        else:
            if self._try_lock_vertical(is_palm_facing_fingertip_up, is_back_facing_fingertip_down):
                return
            if self._try_lock_horizontal(is_horizontal_fingertip_left, is_horizontal_fingertip_right):
                return

        self.palm_stable_frames = 0
        self.back_stable_frames = 0
        self.horizontal_left_stable_frames = 0
        self.horizontal_right_stable_frames = 0
        self.palm_lost_since = None
        self.back_lost_since = None
        self.horizontal_left_lost_since = None
        self.horizontal_right_lost_since = None
        self.swipe_start_tip_y = None
        self.swipe_start_frame = None
        self.fist_start_open_deg = None
        self.fist_start_frame = None
        self.palm_action_cooldown_until = 0
        self.swipe_up_start_tip_y = None
        self.swipe_up_start_frame = None
        self._clear_horizontal_swipe_history()
        self.horizontal_swipe_cooldown_until = 0

    # 重置跟踪状态
    def _reset_tracking_state(self):
        self.hand_landmarks = []
        self.handedness = None
        self.locked_state = GestureState.NONE
        self.no_hand_frames = 0
        self.last_hand_frame_index = -1
        self.palm_stable_frames = 0
        self.back_stable_frames = 0
        self.horizontal_left_stable_frames = 0
        self.horizontal_right_stable_frames = 0
        self.avg_finger_open_deg = None
        self.palm_normal = None
        self.palm_axis_y_angle_deg = None
        self.palm_lost_since = None
        self.back_lost_since = None
        self.horizontal_left_lost_since = None
        self.horizontal_right_lost_since = None
        self.swipe_start_tip_y = None
        self.swipe_start_frame = None
        self.fist_start_open_deg = None
        self.fist_start_frame = None
        self.palm_action_cooldown_until = 0
        self.swipe_up_start_tip_y = None
        self.swipe_up_start_frame = None
        self._clear_horizontal_swipe_history()
        self.horizontal_swipe_cooldown_until = 0
        self.action_client.hide_icon()
        self.reset_tracking_state = 0

    def _clear_horizontal_swipe_history(self):
        self.horizontal_swipe_x_history = []

    def _is_palm_facing_fingertip_up(self):
        if self.avg_finger_open_deg is None or self.palm_normal is None:
            return False

        if self.avg_finger_open_deg <= AVG_FINGER_OPEN_MIN_DEG:
            return False

        if self.handedness not in ("Left", "Right"):
            return False

        if not self._is_vertical_pose_common():
            return False

        cam_dir = np.array([0.0, 0.0, -1.0])
        dot = float(np.dot(self.palm_normal, cam_dir))
        cos_palm = np.cos(np.deg2rad(PALM_ANGLE_THRESHOLD_DEG))

        if self.handedness == "Left":
            return dot <= -cos_palm
        return dot >= cos_palm

    def _is_back_facing_fingertip_down(self):
        if self.avg_finger_open_deg is None or self.palm_normal is None:
            return False

        if self.avg_finger_open_deg <= BACK_SWIPE_OPEN_MIN_DEG:
            return False

        if self.handedness not in ("Left", "Right"):
            return False

        if not self._is_vertical_pose_common(
            min_open_deg=BACK_SWIPE_OPEN_MIN_DEG,
            max_axis_deg=BACK_VERTICAL_AXIS_Y_ANGLE_MAX_DEG,
        ):
            return False

        if not self._is_fingertip_down_posture():
            return False

        cam_dir = np.array([0.0, 0.0, -1.0])
        dot = float(np.dot(self.palm_normal, cam_dir))
        cos_palm = np.cos(np.deg2rad(BACK_SWIPE_PALM_ANGLE_THRESHOLD_DEG))

        if self.handedness == "Left":
            return dot >= cos_palm
        return dot <= -cos_palm

    def _try_lock_vertical(self, is_palm_facing_fingertip_up, is_back_facing_fingertip_down):
        if is_palm_facing_fingertip_up:
            self.palm_stable_frames += 1
            self.back_stable_frames = 0
            self.horizontal_left_stable_frames = 0
            self.horizontal_right_stable_frames = 0
            self.palm_lost_since = None
            self.back_lost_since = None
            self.horizontal_left_lost_since = None
            self.horizontal_right_lost_since = None
            self.swipe_up_start_tip_y = None
            self.swipe_up_start_frame = None
            self._clear_horizontal_swipe_history()
            self.fist_start_open_deg = None
            self.fist_start_frame = None
            if self.palm_stable_frames >= PALM_LOCK_MIN_FRAMES:
                self.locked_state = GestureState.PALM_FACING_FINGERTIP_UP
                self.action_client.show_icon("up")
            return True

        if is_back_facing_fingertip_down:
            self.back_stable_frames += 1
            self.palm_stable_frames = 0
            self.horizontal_left_stable_frames = 0
            self.horizontal_right_stable_frames = 0
            self.back_lost_since = None
            self.palm_lost_since = None
            self.horizontal_left_lost_since = None
            self.horizontal_right_lost_since = None
            self.swipe_start_tip_y = None
            self.swipe_start_frame = None
            self._clear_horizontal_swipe_history()
            if self.back_stable_frames >= PALM_LOCK_MIN_FRAMES:
                self.locked_state = GestureState.BACK_FACING_FINGERTIP_DOWN
                self.action_client.show_icon("down")
            return True

        return False

    def _try_lock_horizontal(self, is_horizontal_fingertip_left, is_horizontal_fingertip_right):
        if is_horizontal_fingertip_left:
            self.horizontal_left_stable_frames += 1
            self.palm_stable_frames = 0
            self.back_stable_frames = 0
            self.horizontal_right_stable_frames = 0
            self.palm_lost_since = None
            self.back_lost_since = None
            self.horizontal_left_lost_since = None
            self.horizontal_right_lost_since = None
            self.swipe_start_tip_y = None
            self.swipe_start_frame = None
            self.swipe_up_start_tip_y = None
            self.swipe_up_start_frame = None
            self._clear_horizontal_swipe_history()
            if self.horizontal_left_stable_frames >= HORIZONTAL_LOCK_MIN_FRAMES:
                if self.locked_state != GestureState.HORIZONTAL_FINGERTIP_LEFT:
                    self._clear_horizontal_swipe_history()
                self.locked_state = GestureState.HORIZONTAL_FINGERTIP_LEFT
                self.action_client.show_icon("right")
            return True

        if is_horizontal_fingertip_right:
            self.horizontal_right_stable_frames += 1
            self.palm_stable_frames = 0
            self.back_stable_frames = 0
            self.horizontal_left_stable_frames = 0
            self.palm_lost_since = None
            self.back_lost_since = None
            self.horizontal_left_lost_since = None
            self.horizontal_right_lost_since = None
            self.swipe_start_tip_y = None
            self.swipe_start_frame = None
            self.swipe_up_start_tip_y = None
            self.swipe_up_start_frame = None
            self._clear_horizontal_swipe_history()
            if self.horizontal_right_stable_frames >= HORIZONTAL_LOCK_MIN_FRAMES:
                if self.locked_state != GestureState.HORIZONTAL_FINGERTIP_RIGHT:
                    self._clear_horizontal_swipe_history()
                self.locked_state = GestureState.HORIZONTAL_FINGERTIP_RIGHT
                self.action_client.show_icon("left")
            return True

        return False

    def _is_horizontal_fingertip_left(self):
        if self._is_back_facing_fingertip_left():
            return True
        if self._is_palm_facing_fingertip_left():
            return True
        return self._is_horizontal_fingertip_left_flipped_handedness()

    def _is_horizontal_fingertip_right(self):
        if self._is_back_facing_fingertip_right():
            return True
        if self._is_palm_facing_fingertip_right():
            return True
        return self._is_horizontal_fingertip_right_flipped_handedness()

    def _flip_handedness(self, hand):
        if hand == "Left":
            return "Right"
        if hand == "Right":
            return "Left"
        return None

    def _is_horizontal_fingertip_left_flipped_handedness(self):
        flipped = self._flip_handedness(self.handedness)
        if flipped is None:
            return False
        if flipped == "Right":
            return self._is_back_facing_fingertip_left_for_hand("Right")
        return self._is_palm_facing_fingertip_left_for_hand("Left")

    def _is_horizontal_fingertip_right_flipped_handedness(self):
        flipped = self._flip_handedness(self.handedness)
        if flipped is None:
            return False
        if flipped == "Left":
            return self._is_back_facing_fingertip_right_for_hand("Left")
        return self._is_palm_facing_fingertip_right_for_hand("Right")

    # 四种横向预备姿态：手背/手心 x 指尖左/右
    def _is_back_facing_fingertip_left(self):
        if self.handedness != "Right":
            return False
        return self._is_back_facing_fingertip_left_for_hand("Right")

    def _is_back_facing_fingertip_right(self):
        if self.handedness != "Left":
            return False
        return self._is_back_facing_fingertip_right_for_hand("Left")

    def _is_palm_facing_fingertip_left(self):
        if self.handedness != "Left":
            return False
        return self._is_palm_facing_fingertip_left_for_hand("Left")

    def _is_palm_facing_fingertip_right(self):
        if self.handedness != "Right":
            return False
        return self._is_palm_facing_fingertip_right_for_hand("Right")

    def _is_back_facing_fingertip_left_for_hand(self, hand):
        if not self._is_horizontal_pose_common(
            min_open_deg=HORIZONTAL_OPEN_MIN_DEG,
            min_axis_deg=HORIZONTAL_AXIS_Y_ANGLE_MIN_DEG,
        ):
            return False
        return self._is_back_facing_camera_for(hand, HORIZONTAL_PALM_ANGLE_THRESHOLD_DEG)

    def _is_back_facing_fingertip_right_for_hand(self, hand):
        if not self._is_horizontal_pose_common(
            min_open_deg=HORIZONTAL_OPEN_MIN_DEG,
            min_axis_deg=HORIZONTAL_AXIS_Y_ANGLE_MIN_DEG,
        ):
            return False
        return self._is_back_facing_camera_for(hand, HORIZONTAL_PALM_ANGLE_THRESHOLD_DEG)

    def _is_palm_facing_fingertip_left_for_hand(self, hand):
        if not self._is_horizontal_pose_common(
            min_open_deg=HORIZONTAL_OPEN_MIN_DEG,
            min_axis_deg=HORIZONTAL_AXIS_Y_ANGLE_MIN_DEG,
        ):
            return False
        return self._is_palm_facing_camera_for(hand, HORIZONTAL_PALM_ANGLE_THRESHOLD_DEG)

    def _is_palm_facing_fingertip_right_for_hand(self, hand):
        if not self._is_horizontal_pose_common(
            min_open_deg=HORIZONTAL_OPEN_MIN_DEG,
            min_axis_deg=HORIZONTAL_AXIS_Y_ANGLE_MIN_DEG,
        ):
            return False
        return self._is_palm_facing_camera_for(hand, HORIZONTAL_PALM_ANGLE_THRESHOLD_DEG)

    def _is_horizontal_pose_common(self, min_open_deg=AVG_FINGER_OPEN_MIN_DEG, min_axis_deg=PALM_AXIS_Y_ANGLE_MIN_DEG):
        if self.avg_finger_open_deg is None:
            return False

        if self.avg_finger_open_deg <= min_open_deg:
            return False

        if self.handedness not in ("Left", "Right"):
            return False

        if self.palm_axis_y_angle_deg is None or self.palm_axis_y_angle_deg < min_axis_deg:
            return False

        return True

    def _is_vertical_pose_common(self, min_open_deg=AVG_FINGER_OPEN_MIN_DEG, max_axis_deg=VERTICAL_AXIS_Y_ANGLE_MAX_DEG):
        if self.avg_finger_open_deg is None:
            return False

        if self.avg_finger_open_deg <= min_open_deg:
            return False

        if self.handedness not in ("Left", "Right"):
            return False

        if self.palm_axis_y_angle_deg is None or self.palm_axis_y_angle_deg > max_axis_deg:
            return False

        return True

    def _is_fingertip_down_posture(self):
        if not self.hand_landmarks:
            return False

        tip_indices = [
            HandLandmark.INDEX_TIP,
            HandLandmark.MIDDLE_TIP,
            HandLandmark.RING_TIP,
            HandLandmark.PINKY_TIP,
        ]
        mcp_indices = [
            HandLandmark.INDEX_MCP,
            HandLandmark.MIDDLE_MCP,
            HandLandmark.RING_MCP,
            HandLandmark.PINKY_MCP,
        ]

        tip_y = float(np.mean([self.hand_landmarks[i].y for i in tip_indices]))
        mcp_y = float(np.mean([self.hand_landmarks[i].y for i in mcp_indices]))
        return (tip_y - mcp_y) >= FINGERTIP_DOWN_MIN_DELTA_Y

    def _is_back_facing_camera(self, angle_threshold_deg=PALM_ANGLE_THRESHOLD_DEG):
        if self.palm_normal is None:
            return False

        if self.handedness not in ("Left", "Right"):
            return False

        return self._is_back_facing_camera_for(self.handedness, angle_threshold_deg)

    def _is_palm_facing_camera(self, angle_threshold_deg=PALM_ANGLE_THRESHOLD_DEG):
        if self.palm_normal is None:
            return False

        if self.handedness not in ("Left", "Right"):
            return False

        return self._is_palm_facing_camera_for(self.handedness, angle_threshold_deg)

    def _is_back_facing_camera_for(self, hand, angle_threshold_deg):
        if self.palm_normal is None:
            return False

        cam_dir = np.array([0.0, 0.0, -1.0])
        dot = float(np.dot(self.palm_normal, cam_dir))
        cos_palm = np.cos(np.deg2rad(angle_threshold_deg))

        if hand == "Left":
            return dot >= cos_palm
        return dot <= -cos_palm

    def _is_palm_facing_camera_for(self, hand, angle_threshold_deg):
        if self.palm_normal is None:
            return False

        cam_dir = np.array([0.0, 0.0, -1.0])
        dot = float(np.dot(self.palm_normal, cam_dir))
        cos_palm = np.cos(np.deg2rad(angle_threshold_deg))

        if hand == "Left":
            return dot <= -cos_palm
        return dot >= cos_palm

    # 下挥手动作判断
    def _update_swipe_down_detection(self):
        tip_y = self._current_tip_y()
        if tip_y is None:
            self.swipe_start_tip_y = None
            self.swipe_start_frame = None
            return

        if self.swipe_start_tip_y is None or self.swipe_start_frame is None:
            self.swipe_start_tip_y = tip_y
            self.swipe_start_frame = self.frame_index
            return

        frame_span = self.frame_index - self.swipe_start_frame
        if frame_span > SWIPE_MAX_FRAMES:
            self.swipe_start_tip_y = tip_y
            self.swipe_start_frame = self.frame_index
            return

        tip_delta = tip_y - self.swipe_start_tip_y
        if tip_delta >= SWIPE_TIP_DELTA_THRESHOLD:
            self.action_client.wheel("down")
            self.swipe_start_tip_y = tip_y
            self.swipe_start_frame = self.frame_index

    # 下挥手和握拳共用同一预备状态，二者在这里竞争判断
    def _update_swipe_down_or_fist_detection(self):
        tip_y = self._current_tip_y()
        if tip_y is None or self.avg_finger_open_deg is None:
            self._reset_palm_action_candidates()
            return

        if self.frame_index < self.palm_action_cooldown_until:
            self.swipe_start_tip_y = tip_y
            self.swipe_start_frame = self.frame_index
            self.fist_start_open_deg = self.avg_finger_open_deg
            self.fist_start_frame = self.frame_index
            return

        if self.swipe_start_tip_y is None or self.swipe_start_frame is None:
            self.swipe_start_tip_y = tip_y
            self.swipe_start_frame = self.frame_index

        if self.fist_start_open_deg is None or self.fist_start_frame is None:
            self.fist_start_open_deg = self.avg_finger_open_deg
            self.fist_start_frame = self.frame_index

        swipe_frame_span = self.frame_index - self.swipe_start_frame
        fist_frame_span = self.frame_index - self.fist_start_frame
        if swipe_frame_span > SWIPE_MAX_FRAMES or fist_frame_span > FIST_MAX_FRAMES:
            self.swipe_start_tip_y = tip_y
            self.swipe_start_frame = self.frame_index
            self.fist_start_open_deg = self.avg_finger_open_deg
            self.fist_start_frame = self.frame_index
            return

        tip_delta = tip_y - self.swipe_start_tip_y
        open_drop = self.fist_start_open_deg - self.avg_finger_open_deg
        swipe_progress = max(tip_delta / SWIPE_TIP_DELTA_THRESHOLD, 0.0)
        fist_required_drop = max(self.fist_start_open_deg - FIST_CLOSED_MAX_DEG, FIST_OPEN_DROP_MIN_DEG)
        fist_progress = max(open_drop / fist_required_drop, 0.0)

        fist_start_ok = self.fist_start_open_deg >= FIST_START_OPEN_MIN_DEG
        fist_closed_now = self.avg_finger_open_deg <= FIST_CLOSED_MAX_DEG
        fist_drop_enough = open_drop >= FIST_OPEN_DROP_MIN_DEG
        swipe_reached = swipe_progress >= 1.0

        # 握拳要求“手指明显收拢”，且不能只是下挥导致的短时形变。
        fist_ready = fist_start_ok and (
            (fist_closed_now and open_drop >= FIST_OPEN_DROP_MIN_DEG * 0.7)
            or (fist_drop_enough and tip_delta <= SWIPE_TIP_DELTA_THRESHOLD * 0.9)
        )

        # 下挥要求“位移主导”，若手指已非常接近闭合则让位给握拳。
        swipe_ready = swipe_reached and (
            open_drop <= FIST_OPEN_DROP_MIN_DEG * 0.75
            or self.avg_finger_open_deg >= (FIST_CLOSED_MAX_DEG + 12.0)
        )

        if fist_ready and swipe_ready:
            very_strong_fist = fist_closed_now and open_drop >= FIST_OPEN_DROP_MIN_DEG
            if very_strong_fist:
                self._on_ptrsc_triggered()
            else:
                self.action_client.wheel("down")
            self.palm_action_cooldown_until = self.frame_index + PALM_ACTION_COOLDOWN_FRAMES
            self._reset_palm_action_candidates()
            return

        if fist_ready:
            self._on_ptrsc_triggered()
            return

        if swipe_ready:
            self.action_client.wheel("down")
            self.palm_action_cooldown_until = self.frame_index + PALM_ACTION_COOLDOWN_FRAMES
            self._reset_palm_action_candidates()
            return

    def _reset_palm_action_candidates(self):
        self.swipe_start_tip_y = None
        self.swipe_start_frame = None
        self.fist_start_open_deg = None
        self.fist_start_frame = None

    def _release_palm_lock_for_ptrsc(self):
        self.locked_state = GestureState.NONE
        self.palm_stable_frames = 0
        self.palm_lost_since = None

    def _on_ptrsc_triggered(self):
        self.action_client.hide_icon()
        self._release_palm_lock_for_ptrsc()
        self.palm_action_cooldown_until = self.frame_index + PALM_ACTION_COOLDOWN_FRAMES
        self._reset_palm_action_candidates()
        self.action_client.ptrsc()

    # 上挥手动作判断
    def _update_swipe_up_detection(self):
        tip_y = self._current_tip_y()
        if tip_y is None:
            self.swipe_up_start_tip_y = None
            self.swipe_up_start_frame = None
            return

        if self.swipe_up_start_tip_y is None or self.swipe_up_start_frame is None:
            self.swipe_up_start_tip_y = tip_y
            self.swipe_up_start_frame = self.frame_index
            return

        frame_span = self.frame_index - self.swipe_up_start_frame
        if frame_span > SWIPE_MAX_FRAMES:
            self.swipe_up_start_tip_y = tip_y
            self.swipe_up_start_frame = self.frame_index
            return

        tip_delta = self.swipe_up_start_tip_y - tip_y
        if tip_delta >= SWIPE_TIP_DELTA_THRESHOLD:
            self.action_client.wheel("up")
            self.swipe_up_start_tip_y = tip_y
            self.swipe_up_start_frame = self.frame_index

    # 左挥手动作判断
    def _update_swipe_left_detection(self):
        self._update_horizontal_swipe_stroke("left")

    # 右挥手动作判断
    def _update_swipe_right_detection(self):
        self._update_horizontal_swipe_stroke("right")

    def _update_horizontal_swipe_stroke(self, direction):
        if self.frame_index < self.horizontal_swipe_cooldown_until:
            return

        track_x = self._current_horizontal_track_x()
        if track_x is None:
            self._clear_horizontal_swipe_history()
            return

        history = self.horizontal_swipe_x_history
        history.append(track_x)
        if len(history) > HORIZONTAL_SWIPE_MAX_FRAMES:
            history.pop(0)

        if len(history) < HORIZONTAL_SWIPE_MIN_FRAMES:
            return

        split = max(1, len(history) // 2)
        baseline = history[:split]
        ref_high = max(baseline)
        ref_low = min(baseline)
        current_x = history[-1]

        triggered = False
        if direction == "left":
            triggered = (ref_high - current_x) >= HORIZONTAL_SWIPE_TIP_DELTA_THRESHOLD
        else:
            triggered = (current_x - ref_low) >= HORIZONTAL_SWIPE_TIP_DELTA_THRESHOLD

        if not triggered:
            return

        self.action_client.horizontal(direction)
        self.horizontal_swipe_cooldown_until = (
            self.frame_index + HORIZONTAL_SWIPE_COOLDOWN_FRAMES
        )
        self._clear_horizontal_swipe_history()

    def _current_horizontal_track_x(self):
        if not self.hand_landmarks:
            return None

        wrist_x = self.hand_landmarks[HandLandmark.WRIST].x
        middle_mcp_x = self.hand_landmarks[HandLandmark.MIDDLE_MCP].x
        tip_x = self._current_tip_x()
        if tip_x is None:
            return None

        # 腕部+掌根比纯指尖更稳定，横向姿态下指尖单独抖动较大
        return 0.45 * wrist_x + 0.25 * middle_mcp_x + 0.30 * tip_x

    # 计算当前帧的手部参数：平均手指伸展角度、掌心法线、掌心y轴角度
    def _compute_frame_params(self):
        self.avg_finger_open_deg = None
        self.palm_normal = None
        self.palm_axis_y_angle_deg = None

        if not self.hand_landmarks:
            return

        def to_np(i):
            lm = self.hand_landmarks[i]
            return np.array([lm.x, lm.y, lm.z], dtype=float)

        finger_triplets = [
            (HandLandmark.INDEX_MCP, HandLandmark.INDEX_PIP, HandLandmark.INDEX_DIP, HandLandmark.INDEX_TIP),
            (HandLandmark.MIDDLE_MCP, HandLandmark.MIDDLE_PIP, HandLandmark.MIDDLE_DIP, HandLandmark.MIDDLE_TIP),
            (HandLandmark.RING_MCP, HandLandmark.RING_PIP, HandLandmark.RING_DIP, HandLandmark.RING_TIP),
            (HandLandmark.PINKY_MCP, HandLandmark.PINKY_PIP, HandLandmark.PINKY_DIP, HandLandmark.PINKY_TIP),
        ]

        finger_opens = []
        for mcp_i, pip_i, dip_i, tip_i in finger_triplets:
            mcp = to_np(mcp_i)
            pip = to_np(pip_i)
            dip = to_np(dip_i)
            tip = to_np(tip_i)

            # 计算PIP和DIP关节的伸展角度
            pip_angle = self._joint_angle_deg(mcp - pip, dip - pip)
            dip_angle = self._joint_angle_deg(pip - dip, tip - dip)
            if pip_angle is None or dip_angle is None:
                continue

            # 取PIP和DIP角度的平均值作为该手指的伸展程度
            finger_open = (pip_angle + dip_angle) / 2.0
            finger_opens.append(finger_open)

        if not finger_opens:
            return

        self.avg_finger_open_deg = float(np.mean(finger_opens))

        wrist = to_np(HandLandmark.WRIST)
        idx_mcp = to_np(HandLandmark.INDEX_MCP)
        pinky_mcp = to_np(HandLandmark.PINKY_MCP)

        v1 = idx_mcp - wrist
        v2 = pinky_mcp - wrist
        normal = np.cross(v1, v2)

        norm = np.linalg.norm(normal)
        if norm == 0:
            return
        self.palm_normal = normal / norm

        middle_mcp = to_np(HandLandmark.MIDDLE_MCP)
        axis_2d = np.array([middle_mcp[0] - wrist[0], middle_mcp[1] - wrist[1]], dtype=float)
        axis_norm = np.linalg.norm(axis_2d)
        if axis_norm > 0:
            y_axis = np.array([0.0, 1.0], dtype=float)
            cos_axis_y = float(np.dot(axis_2d, y_axis) / axis_norm)
            cos_axis_y = np.clip(cos_axis_y, -1.0, 1.0)
            # 使用绝对值将夹角折叠为[0,90]，便于区分纵向/横向姿态
            self.palm_axis_y_angle_deg = float(np.degrees(np.arccos(abs(cos_axis_y))))
        else:
            self.palm_axis_y_angle_deg = None

    # 获取当前帧的指尖平均y坐标
    def _current_tip_y(self):
        if not self.hand_landmarks:
            return None

        tip_indices = [
            HandLandmark.INDEX_TIP,
            HandLandmark.MIDDLE_TIP,
            HandLandmark.RING_TIP,
            HandLandmark.PINKY_TIP,
        ]
        y_values = [self.hand_landmarks[i].y for i in tip_indices]
        return float(np.mean(y_values))

    # 获取当前帧的指尖平均x坐标
    def _current_tip_x(self):
        if not self.hand_landmarks:
            return None

        tip_indices = [
            HandLandmark.INDEX_TIP,
            HandLandmark.MIDDLE_TIP,
            HandLandmark.RING_TIP,
            HandLandmark.PINKY_TIP,
        ]
        x_values = [self.hand_landmarks[i].x for i in tip_indices]
        return float(np.mean(x_values))

    # 计算两个向量之间的夹角（单位：度）
    def _joint_angle_deg(self, vec_a, vec_b):
        norm_a = np.linalg.norm(vec_a)
        norm_b = np.linalg.norm(vec_b)
        if norm_a == 0 or norm_b == 0:
            return None

        cos_val = np.dot(vec_a, vec_b) / (norm_a * norm_b)
        cos_val = np.clip(cos_val, -1.0, 1.0)
        return float(np.degrees(np.arccos(cos_val)))
