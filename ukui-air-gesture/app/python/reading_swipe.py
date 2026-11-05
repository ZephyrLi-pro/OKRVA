"""Open-palm horizontal strokes for a reader, independent of finger direction.

Coordinates are normalized to frame width and mirrored like the preview.
No desktop actions occur here. A caller decides where recognized strokes go.
"""
from collections import deque
import math

COOLDOWN_SECONDS = .5


def palm_sample_info(landmarks, aspect=.75, *, allow_edge=False):
    if landmarks is None or len(landmarks) != 21:
        return None, '未检测到完整的手部关键点'
    points = [(float(p.x), float(p.y), float(p.z)) for p in landmarks]
    if not all(math.isfinite(v) for p in points for v in p):
        return None, '关键点无效，等待重新跟踪'
    # All motion anchors must be visible. Unused wrist/thumb predictions may
    # leave the frame while the palm and extended fingers remain observable.
    ids = (5,9,13,17)
    visible_anchors=sum(.02<=points[i][0]<=.98 and .02<=points[i][1]<=.98 for i in ids)
    if visible_anchors<4:
        # Only a controller with a very recent fully visible hand may request
        # this continuation. Keep two visible palm anchors and fingers, and
        # permit at most a small prediction margin beyond the frame.
        if (not allow_edge or visible_anchors<2 or any(
                not (-.05<=points[i][0]<=1.05 and .02<=points[i][1]<=.98) for i in ids)):
            return None, '手掌主体靠近边缘，请移回画面中央'
    span = max(points[i][0] for i in ids)-min(points[i][0] for i in ids)
    if not .04 <= span <= .85:
        return None, '手部尺寸不稳定，请调整与镜头的距离'

    def angle(a, b, c):
        va = tuple(points[a][i]-points[b][i] for i in range(3))
        vb = tuple(points[c][i]-points[b][i] for i in range(3))
        norm = math.sqrt(sum(v*v for v in va)*sum(v*v for v in vb))
        if norm < 1e-10:
            return 0.0
        return math.degrees(math.acos(max(-1.,min(1.,sum(x*y for x,y in zip(va,vb))/norm))))

    fingers = [(5,6,7,8),(9,10,11,12),(13,14,15,16),(17,18,19,20)]
    finger_left,finger_right=(0.,1.) if allow_edge else (.01,.99)
    visible = [finger for finger in fingers if all(
        finger_left <= points[i][0] <= finger_right and .01 <= points[i][1] <= .99 for i in finger)]
    if len(visible) < (2 if allow_edge else 3):
        return None, '可见手指不足，请让手掌和手指进入画面'
    openness = sum((angle(m,p,d)+angle(p,d,t))/2 for m,p,d,t in visible)/len(visible)
    return (1-sum(points[i][0] for i in ids)/len(ids),
            aspect*sum(points[i][1] for i in ids)/len(ids), openness), None


def palm_sample(landmarks, aspect=.75):
    return palm_sample_info(landmarks,aspect)[0]


class ReadingSwipeDetector:
    def __init__(self, minimum_distance=.12, vertical_tolerance=0., vertical_ratio=.5):
        if not .03 <= minimum_distance <= .2:
            raise ValueError('Invalid horizontal stroke distance')
        self.minimum_distance = float(minimum_distance)
        if not 0<=vertical_tolerance<=.1:raise ValueError('Invalid vertical tolerance')
        self.vertical_tolerance=float(vertical_tolerance)
        if not .3<=vertical_ratio<=.8:raise ValueError('Invalid vertical ratio')
        self.vertical_ratio=float(vertical_ratio)
        self.history = deque()
        self.last_time = None
        self.last_point = None
        self.last_trigger = -math.inf
        self.armed = True
        self.quiet_since = None
        self.quiet_anchor = None
        self.last_direction = None
        self.extreme = None
        self.gate = 'await_hand'
        self.reason = '等待完整张开的手掌'

    def reset_track(self):
        self.history.clear()
        self.last_point = None
        self.quiet_since = None
        self.quiet_anchor = None
        self.last_direction = None
        self.extreme = None
        self.armed = True

    def prime(self, observations, aspect=.75):
        """Seed trusted neutral-zone observations without emitting a stroke."""
        self.reset_track()
        self.last_time = None
        for landmarks,now in observations:
            sample,_ = palm_sample_info(landmarks,aspect)
            if sample is None or sample[2]<140 or not math.isfinite(now):
                self.reset_track()
                continue
            if self.last_time is not None:
                if now<=self.last_time:
                    continue
                if now-self.last_time>.5:
                    self.reset_track()
            x,y,_=sample
            self.history.append((now,x,y))
            self.last_point=(x,y)
            self.last_time=now
        self.gate='primed'

    def _rest_observed(self, previous, previous_time, x, y, now):
        if previous is None:
            self.quiet_since = None
            self.quiet_anchor = None
            return False
        if self.quiet_anchor is None:
            self.quiet_anchor = previous
            self.quiet_since = previous_time
        # Measure net movement around a fixed anchor, not small step sizes.
        # A slowly moving hand must not look like a stationary hand.
        if math.hypot(x-self.quiet_anchor[0],y-self.quiet_anchor[1]) > .018:
            self.quiet_anchor = (x,y)
            self.quiet_since = now
        return self.quiet_since is not None and now-self.quiet_since >= .25

    def update(self, landmarks, now, aspect=.75):
        if not math.isfinite(now) or (self.last_time is not None and now <= self.last_time):
            return None
        previous_time = self.last_time
        gap = self.last_time is not None and now-self.last_time > .5
        self.last_time = now
        sample,reason = palm_sample_info(landmarks,aspect)
        if sample is None or sample[2] < 140:
            self.reset_track()
            self.reason = reason or '请张开手掌，避免握拳或弯曲手指'
            self.gate = 'invalid_palm'
            return None
        x,y,_ = sample
        if gap:
            self.reset_track()
        previous = self.last_point
        if previous is not None and math.hypot(x-previous[0],y-previous[1]) > .22:
            self.reset_track()
            previous = None
        self.last_point = x,y
        if not self.armed:
            if self._rest_observed(previous,previous_time,x,y,now):
                self.armed = True
                self.history.clear()
                self.history.append((now,x,y))
                self.quiet_since = None
                self.quiet_anchor = None
                self.last_direction = None
                self.extreme = None
            else:
                continuing = (self.extreme is None
                    or (self.last_direction=='left' and x<=self.extreme[1])
                    or (self.last_direction=='right' and x>=self.extreme[1]))
                if continuing or now-self.extreme[0]>1.8:
                    self.extreme = (now,x,y)
                    self.history.clear()
                    self.history.append(self.extreme)
                # A reversal starts at the last extreme. It can become the
                # next opposite stroke without forcing a stationary hold.
        if not self.history or self.history[-1][0] != now:
            self.history.append((now,x,y))
        while self.history and now-self.history[0][0] > 1.8:
            self.history.popleft()
        if now-self.last_trigger < COOLDOWN_SECONDS:
            self.gate = 'cooldown'
            self.reason = f'冷却剩余{COOLDOWN_SECONDS-(now-self.last_trigger):.1f}秒，可准备反向挥动'
            return None
        if len(self.history)<3:
            self.gate = 'collecting'
            self.reason = ('本次已计数，可反向挥动或短暂停稳' if not self.armed and len(self.history)==1
                           else f'正在收集轨迹：{len(self.history)}/3个采样')
            return None
        if now-self.history[0][0] < .15:
            self.gate = 'too_short'
            self.reason = '动作时间过短，等待下一次采样'
            return None
        _,start_x,start_y = self.history[0]
        dx,dy = x-start_x,y-start_y
        distance = abs(dx)
        travel = sum(abs(self.history[i][1]-self.history[i-1][1]) for i in range(1,len(self.history)))
        y_range = max(p[2] for p in self.history)-min(p[2] for p in self.history)
        if distance + 1e-8 < self.minimum_distance:
            self.gate = 'small_distance'
            self.reason = f'水平幅度{distance*100:.0f}% / {self.minimum_distance*100:.0f}%，请继续同向移动'
            return None
        if (abs(dy) > max(self.vertical_tolerance,distance*self.vertical_ratio)
                or y_range > max(.06,distance*max(.55,self.vertical_ratio))):
            self.gate = 'vertical_motion'
            self.reason = '上下偏移较大，请尽量横向挥动'
            return None
        if travel <= 0 or distance/travel < .8:
            self.gate = 'mixed_direction'
            self.reason = '轨迹来回折返，正在分辨挥动方向'
            return None
        direction = 'left' if dx<0 else 'right'
        if not self.armed and direction==self.last_direction:
            self.gate = 'same_stroke'
            self.reason = '本次已计数，可反向挥动或短暂停稳'
            return None
        self.last_trigger = now
        self.armed = False
        self.quiet_since = None
        self.quiet_anchor = None
        self.last_direction = direction
        self.extreme = (now,x,y)
        self.history.clear()
        self.history.append(self.extreme)
        self.gate = 'triggered'
        self.reason = '已识别，可反向挥动；同向下一次请稍停'
        return direction

    def diagnostics(self, now):
        dx=self.history[-1][1]-self.history[0][1] if self.history else 0.
        dy=self.history[-1][2]-self.history[0][2] if self.history else 0.
        return {'reading_armed':self.armed,'decision_gate':self.gate,
                'last_trigger_direction':self.last_direction,
                'horizontal_displacement':round(dx,4),'vertical_displacement':round(dy,4),
                'cooldown_remaining_seconds':round(max(0.,COOLDOWN_SECONDS-(now-self.last_trigger)),3),
                'history_samples':len(self.history),
                'quiet_seconds':round(max(0.,now-self.quiet_since),3) if self.quiet_since is not None else 0.}
