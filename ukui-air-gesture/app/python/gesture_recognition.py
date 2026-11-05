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
import cv2
import time
import numpy as np
from collections import deque
from dataclasses import dataclass
from pathlib import Path

from gesture_classifier import GestureClassifier
from opencv_ort_backend import ORTHandPose, ORTPalmDet
from virtual_camera import (
    activate_shared_camera,
    find_real_camera_node,
    open_loopback_writer,
)

# 摄像头采集配置
CAMERA_FRAME_WIDTH = 640
CAMERA_FRAME_HEIGHT = 480
CAMERA_FPS = 10
SHOW_PREVIEW = False        # 后续无窗口模式时可保持False

# 每隔N帧进行检测识别 跳帧处理 若下游触发变迟钝可设为 1
DETECT_EVERY_N_FRAME = 1

# Once a hand is found, reuse the previous frame's palm landmarks as the ROI
# for the hand-pose model.  A full-frame palm detection is used again when
# tracking fails, or periodically as a guard against long-term drift.
TRACKING_REFRESH_FRAMES = 60
TRACKING_MIN_PALM_SIZE_PX = 12
TRACKING_MAX_PALM_SIZE_RATIO = 0.95
TRACKING_STATS_INTERVAL = 60
PALM_SEARCH_INTERVAL_SEC = 0.4
PALM_IDLE_AFTER_SEC = 2.0
PALM_IDLE_SEARCH_INTERVAL_SEC = 3.0
PALM_DETECTION_SCORE_THRESHOLD = 0.6

# Accumulate only high-quality handedness samples.  A hand near an image edge
# or with a weak pose score contributes little or no evidence.  Once locked,
# handedness remains fixed for the lifetime of the hand session.
HANDEDNESS_LEFT_CONFIDENT_MAX = 0.35
HANDEDNESS_RIGHT_CONFIDENT_MIN = 0.65
HANDEDNESS_QUALITY_FLOOR = 0.25
HANDEDNESS_MIN_TOTAL_WEIGHT = 0.8
HANDEDNESS_MIN_CONFIDENT_SAMPLES = 3
HANDEDNESS_DOMINANCE_RATIO = 0.75
HANDEDNESS_EVIDENCE_WINDOW = 8
HANDEDNESS_FORGET_FRAMES = 30
HANDEDNESS_EDGE_MARGIN_RATIO = 0.04
GESTURE_WARMUP_BUFFER_FRAMES = 12

PALM_LANDMARK_IDS = np.array([0, 5, 9, 13, 17, 1, 2])


@dataclass
class Landmark:
    x: float
    y: float
    z: float


class HandednessStabilizer:
    """Lock handedness from a quality-weighted window of observations."""

    def __init__(self):
        self.current = None
        self.samples = deque(maxlen=HANDEDNESS_EVIDENCE_WINDOW)
        self.missing_frames = 0

    def update(self, score, quality=1.0):
        score = float(np.clip(score, 0.0, 1.0))
        quality = float(np.clip(quality, 0.0, 1.0))
        self.missing_frames = 0

        if self.current is not None:
            return self.current, None

        # Edge-clipped landmarks are less trustworthy, but discarding them
        # completely can prevent a label forever when one fingertip touches
        # the image boundary.  Keep a small evidence floor and let several
        # consistent observations outvote an isolated bad first frame.
        weight = max(quality, HANDEDNESS_QUALITY_FLOOR)
        if score <= HANDEDNESS_LEFT_CONFIDENT_MAX:
            side = "Left"
        elif score >= HANDEDNESS_RIGHT_CONFIDENT_MIN:
            side = "Right"
        else:
            side = None
            weight = 0.0

        self.samples.append((side, weight))
        confident_samples = [
            (sample_side, sample_weight)
            for sample_side, sample_weight in self.samples
            if sample_side is not None
        ]
        if len(confident_samples) < HANDEDNESS_MIN_CONFIDENT_SAMPLES:
            return None, None

        total_weight = sum(weight for _, weight in confident_samples)
        if total_weight < HANDEDNESS_MIN_TOTAL_WEIGHT:
            return None, None

        left_weight = sum(
            weight for side, weight in confident_samples if side == "Left"
        )
        right_weight = total_weight - left_weight
        left_count = sum(side == "Left" for side, _ in confident_samples)
        right_count = sum(side == "Right" for side, _ in confident_samples)
        if (
            left_count >= HANDEDNESS_MIN_CONFIDENT_SAMPLES
            and left_weight / total_weight >= HANDEDNESS_DOMINANCE_RATIO
        ):
            self.current = "Left"
        elif (
            right_count >= HANDEDNESS_MIN_CONFIDENT_SAMPLES
            and right_weight / total_weight >= HANDEDNESS_DOMINANCE_RATIO
        ):
            self.current = "Right"
        else:
            return None, None

        self.samples.clear()
        return self.current, "locked"

    def mark_missing(self):
        self.missing_frames += 1
        self.samples.clear()
        if self.missing_frames < HANDEDNESS_FORGET_FRAMES:
            return False

        forgotten = self.current is not None
        self.current = None
        return forgotten


class BufferedGestureDispatcher:
    """Replay warm-up landmarks after handedness becomes reliable."""

    def __init__(self, classifier):
        self.classifier = classifier
        self.pending = deque(maxlen=GESTURE_WARMUP_BUFFER_FRAMES)

    def classify(self, landmarks, handedness, frame_index):
        if landmarks is None:
            self.pending.clear()
            self.classifier.classify(None, None, frame_index)
            return

        if handedness is None:
            self.pending.append((landmarks, frame_index))
            return

        while self.pending:
            buffered_landmarks, buffered_frame_index = self.pending.popleft()
            self.classifier.classify(
                buffered_landmarks, handedness, buffered_frame_index
            )
        self.classifier.classify(landmarks, handedness, frame_index)


class HandTracker:
    """Run full-frame palm detection only when hand-pose tracking needs it."""

    def __init__(self, palm_detector, pose_detector):
        self.palm_detector = palm_detector
        self.pose_detector = pose_detector
        self.tracking_palm = None
        self.tracked_frames = 0
        self.total_frames = 0
        self.palm_runs = 0
        self.pose_runs = 0
        self.track_hits = 0
        self.redetects = 0
        self.search_skips = 0
        self.next_search_at = 0.0
        self.last_search_at = -float('inf')
        self.empty_since = None
        self.search_reference = None
        self.handedness = HandednessStabilizer()
        self.last_handedness_score = None
        self.last_handedness_quality = None
        self.started_at = time.monotonic()

    def reset(self):
        self.tracking_palm = None
        self.tracked_frames = 0

    def _valid_hand(self, hand, image_shape):
        if hand is None or len(hand) < 132 or not np.all(np.isfinite(hand)):
            return False

        height, width = image_shape[:2]
        points = hand[4:67].reshape(21, 3)[:, :2]
        palm_points = points[PALM_LANDMARK_IDS]
        palm_size = np.ptp(palm_points, axis=0)
        min_size = float(np.min(palm_size))
        max_size = float(np.max(palm_size))

        if min_size < TRACKING_MIN_PALM_SIZE_PX:
            return False
        if max_size > max(width, height) * TRACKING_MAX_PALM_SIZE_RATIO:
            return False

        # Permit a small amount of overshoot because landmarks near an image
        # edge can legitimately fall just outside the visible frame.
        margin_x = width * 0.15
        margin_y = height * 0.15
        if (np.max(palm_points[:, 0]) < -margin_x
                or np.min(palm_points[:, 0]) > width + margin_x
                or np.max(palm_points[:, 1]) < -margin_y
                or np.min(palm_points[:, 1]) > height + margin_y):
            return False

        return True

    def _palm_from_hand(self, hand):
        points = hand[4:67].reshape(21, 3)[:, :2]
        palm_points = points[PALM_LANDMARK_IDS]
        bbox = np.r_[np.min(palm_points, axis=0), np.max(palm_points, axis=0)]
        return np.r_[bbox, palm_points.reshape(-1), hand[-1]]

    @staticmethod
    def _handedness_quality(hand, image_shape):
        height, width = image_shape[:2]
        points = hand[4:67].reshape(21, 3)[:, :2]
        edge_distance = min(
            float(np.min(points[:, 0])),
            float(width - np.max(points[:, 0])),
            float(np.min(points[:, 1])),
            float(height - np.max(points[:, 1])),
        )
        target_margin = max(
            min(width, height) * HANDEDNESS_EDGE_MARGIN_RATIO,
            1.0,
        )
        edge_quality = float(np.clip(edge_distance / target_margin, 0.0, 1.0))
        pose_quality = float(np.clip(hand[-1], 0.0, 1.0))
        return pose_quality * edge_quality

    def _run_pose(self, frame, palm):
        self.pose_runs += 1
        hand = self.pose_detector.infer(frame, palm)
        return hand if self._valid_hand(hand, frame.shape) else None

    def _run_full_detection(self, frame):
        self.palm_runs += 1
        palms = self.palm_detector.infer(frame)
        if len(palms) == 0:
            return None
        return self._run_pose(frame, palms[0])

    @staticmethod
    def _search_thumbnail(frame):
        # Kept in memory only. Coarse frame changes wake the palm model before
        # the quiet fallback probe; this is never a hand/gesture classifier.
        small = cv2.resize(frame, (64, 48), interpolation=cv2.INTER_NEAREST)
        return cv2.cvtColor(small, cv2.COLOR_BGR2GRAY).astype(np.int16)

    def _search_scene_changed(self, frame):
        if self.search_reference is None:
            return True
        delta = self._search_thumbnail(frame) - self.search_reference
        # Ignore a uniform exposure adjustment and isolated sensor noise.
        delta -= int(np.median(delta))
        return np.count_nonzero(np.abs(delta) > 12) >= delta.size * .015

    def _accept_hand(self, hand):
        self.tracking_palm = self._palm_from_hand(hand)
        self.tracked_frames += 1

    def _report_stats(self):
        if self.total_frames % TRACKING_STATS_INTERVAL != 0:
            return
        elapsed = max(time.monotonic() - self.started_at, 0.001)
        model_runs = self.palm_runs + self.pose_runs
        print(
            "Tracker stats: "
            f"frames={self.total_frames} capture_fps={self.total_frames / elapsed:.2f} "
            f"model_fps={model_runs / elapsed:.2f} "
            f"palm_runs={self.palm_runs} pose_runs={self.pose_runs} "
            f"track_hits={self.track_hits} redetects={self.redetects} "
            f"search_skips={self.search_skips} "
            f"handedness={self.handedness.current or 'none'} "
            f"handedness_score={self.last_handedness_score} "
            f"handedness_quality={self.last_handedness_quality}"
        )

    def infer(self, frame, now=None):
        self.total_frames += 1
        explicit_clock = now is not None
        now = time.monotonic() if now is None else float(now)
        had_track = self.tracking_palm is not None

        if not had_track and now < self.next_search_at:
            changed = (now - self.last_search_at >= PALM_SEARCH_INTERVAL_SEC
                       and self._search_scene_changed(frame))
            if not changed:
                self.search_skips += 1
                if self.handedness.mark_missing():
                    print("Handedness lock expired after prolonged hand loss")
                self._report_stats()
                return None, None, "search-wait"

        if not had_track:
            self.last_search_at = now

        refresh_due = had_track and self.tracked_frames >= TRACKING_REFRESH_FRAMES
        hand = None
        mode = "none"

        if had_track and not refresh_due:
            hand = self._run_pose(frame, self.tracking_palm)
            if hand is not None:
                self.track_hits += 1
                mode = "tracking"

        if hand is None:
            if had_track:
                self.redetects += 1
            hand = self._run_full_detection(frame)
            if hand is not None:
                self.tracked_frames = 0
                mode = "refresh" if refresh_due else "detection"
            elif refresh_due:
                # A periodic refresh is a safety check, not a reason to lose a
                # valid track.  Try the old ROI once before declaring loss.
                hand = self._run_pose(frame, self.tracking_palm)
                if hand is not None:
                    self.track_hits += 1
                    self.tracked_frames = 0
                    mode = "tracking"

        if hand is None:
            self.reset()
            if self.empty_since is None:
                self.empty_since = now
            self.search_reference = self._search_thumbnail(frame)
            if now - self.empty_since >= PALM_IDLE_AFTER_SEC:
                # The quiet interval begins AFTER inference. Starting it before
                # a slow detector pass can otherwise leave no idle time at all.
                completed = now if explicit_clock else time.monotonic()
                self.next_search_at = completed + PALM_IDLE_SEARCH_INTERVAL_SEC
            else:
                # Keep the original fast reacquisition cadence after hand loss.
                self.next_search_at = now + PALM_SEARCH_INTERVAL_SEC
            if self.handedness.mark_missing():
                print("Handedness lock expired after prolonged hand loss")
            self._report_stats()
            return None, None, mode

        self.empty_since = None
        self.search_reference = None
        self._accept_hand(hand)
        points = hand[4:67].reshape(21, 3)
        height, width = frame.shape[:2]
        scale = float(max(width, height))
        landmarks = [
            Landmark(float(x / width), float(y / height), float(z / scale))
            for x, y, z in points
        ]
        self.last_handedness_score = round(float(hand[-2]), 4)
        handedness_quality = self._handedness_quality(hand, frame.shape)
        self.last_handedness_quality = round(handedness_quality, 4)
        handedness, handedness_event = self.handedness.update(
            hand[-2], handedness_quality
        )
        if handedness_event is not None:
            print(
                f"Handedness {handedness_event}: {handedness} "
                f"(score={self.last_handedness_score:.4f})"
            )
        self._report_stats()
        return landmarks, handedness, mode

# 解析模型文件路径
def resolve_model_path(model_name):
    script_dir = Path(__file__).resolve().parent
    model_path = script_dir.parent / "models" / model_name

    if model_path.exists():
        return str(model_path)

    print(f"Model file not found: {model_path}")
    return None

# 初始化手势识别配置
def initialize_hand_detector():
    palm_model = resolve_model_path("palm_detection_mediapipe_2023feb.onnx")
    hand_model = resolve_model_path("handpose_estimation_mediapipe_2023feb.onnx")
    if palm_model is None or hand_model is None:
        return None
    return (
        ORTPalmDet(
            palm_model,
            score_threshold=PALM_DETECTION_SCORE_THRESHOLD,
            nms_threshold=0.3,
        ),
        ORTHandPose(hand_model, confidence_threshold=0.4),
    )


# 缩放图像到合适的尺寸
def resize_image_for_detect(image, max_dim=640):
    h, w = image.shape[:2]
    if max(h, w) <= max_dim:
        return image
    scale = max_dim / max(h, w)
    new_w = int(w * scale)
    new_h = int(h * scale)

    return cv2.resize(image, (new_w, new_h), interpolation=cv2.INTER_AREA)


# 从单帧图像里识别手部坐标
def get_hand_landmarks_from_frame(frame, hand_tracker, _timestamp_ms):
    detect_frame = resize_image_for_detect(frame)
    return hand_tracker.infer(detect_frame)

# 调试使用
def process_frame(landmarks, frame):
    # 使图像水平翻转，使图像实现镜面
    frame = cv2.flip(frame, 1)
    if landmarks:
        h, w = frame.shape[:2] # 获取当前图像宽高
        for lm in landmarks:
            cx = int((1 - lm.x) * w)
            cy = int(lm.y * h)
            color=(0,255,0)
            cv2.circle(frame, (cx, cy), 3, color, -1)
    
    return frame


def configure_camera_capture(cap, width=CAMERA_FRAME_WIDTH, height=CAMERA_FRAME_HEIGHT):
    # 请求设置采集分辨率，驱动可能回退到最接近的支持值
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    cap.set(cv2.CAP_PROP_FPS, CAMERA_FPS)

    actual_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    actual_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    actual_fps = cap.get(cv2.CAP_PROP_FPS)
    print(
        "Camera capture config: "
        f"{actual_width}x{actual_height}, "
        f"fps={actual_fps:.1f}"
    )
    return actual_width, actual_height, actual_fps


def recognize_gesture():
    hand_detector = initialize_hand_detector()
    if hand_detector is None:
        print("Failed to initialize hand detector.")
        return

    activate_shared_camera()

    real_camera = find_real_camera_node()
    if not real_camera:
        print("Failed to find real camera.")
        return

    print(f"Opening real camera: {real_camera}")
    cap = cv2.VideoCapture(real_camera, cv2.CAP_V4L2)
    if not cap.isOpened():
        print(f"Failed to open camera: {real_camera}")
        return

    actual_width, actual_height, actual_fps = configure_camera_capture(cap)
    writer_fps = actual_fps if actual_fps and actual_fps > 0 else CAMERA_FPS
    loopback_writer = open_loopback_writer(actual_width, actual_height, writer_fps)

    classifier = GestureClassifier()
    gesture_dispatcher = BufferedGestureDispatcher(classifier)
    hand_tracker = HandTracker(*hand_detector)

    frame_index = 0
    landmarks = None
    handedness = None
    hand_visible = False
    last_tracker_mode = "none"

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                break

            if loopback_writer is not None:
                if not loopback_writer.write(frame):
                    loopback_writer = None

            # VIDEO 模式：为每帧维护一个严格递增的时间戳（帧号 × 每帧毫秒数）
            timestamp_ms = int(frame_index * 1000 / max(writer_fps, 1))

            # 每隔 DETECT_EVERY_N_FRAME 帧识别一次（首帧也识别）
            if frame_index % DETECT_EVERY_N_FRAME == 0:
                landmarks, handedness, tracker_mode = get_hand_landmarks_from_frame(
                    frame, hand_tracker, timestamp_ms
                )
                detected = landmarks is not None
                if detected != hand_visible:
                    if detected:
                        print(f"Hand detected: {handedness} ({tracker_mode})")
                    else:
                        print("Hand lost")
                    hand_visible = detected
                elif detected and tracker_mode in ("detection", "refresh"):
                    print(f"Tracker reacquired hand: {handedness} ({tracker_mode})")
                if tracker_mode != last_tracker_mode and tracker_mode == "tracking":
                    print("Tracker entered ROI tracking")
                last_tracker_mode = tracker_mode
                # 传入真实帧号，让分类器内部所有帧阈值以真实帧为标准
                gesture_dispatcher.classify(landmarks, handedness, frame_index)

            frame_index += 1

            if SHOW_PREVIEW:
                preview = process_frame(landmarks, frame)
                cv2.imshow("hand", preview)
                key = cv2.waitKey(1) & 0xFF
                if key in (27, ord('q')):
                    break
    finally:
        if loopback_writer is not None:
            loopback_writer.close()
        cap.release()
        cv2.destroyAllWindows()
