"""Center-to-side page commands. Returning to center only rearms the gesture."""
import math
from collections import deque
from reading_swipe import ReadingSwipeDetector, COOLDOWN_SECONDS, palm_sample_info


class ReadingNavigationController:
    CENTER_LEFT=.4
    CENTER_RIGHT=.6
    HOME_HALF_WIDTH=.04
    STROKE_DISTANCE=.10

    def __init__(self):
        self.raw=ReadingSwipeDetector(minimum_distance=.06)
        self.phase='await_center'
        self.last_time=None
        self.previous=None
        self.last_accepted=-math.inf
        self.pending=None
        self.pending_since=None
        self.center_samples=deque(maxlen=3)
        self.neutral_samples=deque(maxlen=32)
        self.origin=.5
        self.reason='请先将掌心圆点放到中间框内'

    def _new_trace(self, observations, aspect):
        self.raw=ReadingSwipeDetector(minimum_distance=.06)
        self.raw.last_trigger=self.last_accepted
        self.raw.prime([(landmarks,now) for landmarks,now,_ in observations],aspect)

    def _at_target(self,direction,x):
        return ((direction=='left' and x<=self.origin-self.STROKE_DISTANCE+1e-8)
                or (direction=='right' and x>=self.origin+self.STROKE_DISTANCE-1e-8))

    def _accept(self,direction,now):
        if now-self.last_accepted<COOLDOWN_SECONDS:
            self.reason='短暂冷却中，请稍候'
            return None
        self.last_accepted=now
        self.raw.last_trigger=now
        self.phase='returning'
        self.pending=None
        self.pending_since=None
        self.center_samples.clear()
        self.reason=('左挥' if direction=='left' else '右挥')+'已计数；回到中间只复位'
        return direction

    def update(self,landmarks,now,aspect=.75):
        if not math.isfinite(now) or (self.last_time is not None and now<=self.last_time):
            return None
        gap=self.last_time is not None and now-self.last_time>.5
        self.last_time=now
        sample,reason=palm_sample_info(landmarks,aspect)
        if sample is None or sample[2]<140:
            self.raw.update(landmarks,now,aspect)
            self.phase='await_center'
            self.previous=None
            self.pending=None
            self.center_samples.clear()
            self.neutral_samples.clear()
            self.origin=.5
            self.reason=reason or '请张开手掌，再回到中间框内'
            return None
        x=sample[0]
        previous=None if gap else self.previous
        self.previous=(landmarks,now,x)
        global_center=self.CENTER_LEFT-1e-8<=x<=self.CENTER_RIGHT+1e-8
        # Learn a resting start position, without following a moving stroke
        # or treating a pause on the return path as a new opposite gesture.
        if global_center and self.phase!='returning':
            if gap:self.neutral_samples.clear()
            self.neutral_samples.append(self.previous)
            while len(self.neutral_samples)>3 and now-self.neutral_samples[1][1]>=.3:
                self.neutral_samples.popleft()
            samples=list(self.neutral_samples)
            if (len(samples)>=3 and samples[-1][1]-samples[0][1]>=.3-1e-8
                    and max(s[2] for s in samples)-min(s[2] for s in samples)<=.018
                    and abs(x-self.origin)<=self.STROKE_DISTANCE+1e-8):
                origin=sum(s[2] for s in samples)/len(samples)
                if abs(origin-self.origin)>.005:
                    self.origin=origin
                    self.phase='ready'
                    self.pending=None
                    self.pending_since=None
                    self.center_samples.clear()
        else:
            self.neutral_samples.clear()
        left=self.origin-self.HOME_HALF_WIDTH
        right=self.origin+self.HOME_HALF_WIDTH
        in_center=(global_center if self.phase=='await_center'
                   else left-1e-8<=x<=right+1e-8)
        if in_center:
            if self.phase!='ready':self.center_samples.clear()
            self.center_samples.append(self.previous)
            self._new_trace(list(self.center_samples),aspect)
            self.phase='ready'
            self.pending=None
            self.pending_since=None
            self.reason='已就绪，可从起始位置向左或向右挥动'
            return None
        if self.phase in ('await_center','returning'):
            crossed=(previous is not None and
                min(previous[2],x)<left and max(previous[2],x)>right)
            if not crossed:
                self.reason=('本次已计数，掌心回中间只复位' if self.phase=='returning'
                             else '请先将掌心圆点放到中间框内')
                return None
            self._new_trace([previous],aspect)
            self.phase='outbound'
        elif self.phase=='ready':
            # Preserve the recent neutral samples aligned with the outward
            # direction, so centering does not add a new stationary delay.
            samples=list(self.center_samples)
            sign=-1 if x<self.origin else 1
            index=len(samples)-1
            while index>0 and sign*(samples[index][2]-samples[index-1][2])>=-.01:
                index-=1
            self._new_trace(samples[index:],aspect)
        self.phase='outbound'
        if self.pending is not None:
            if now-self.pending_since>1.8:
                self.phase='await_center'
                self.pending=None
                self.reason='动作已超时，请回中间重新开始'
                return None
            if self._at_target(self.pending,x):
                return self._accept(self.pending,now)
            self.reason='继续向'+('左' if self.pending=='left' else '右')+'越过标记线'
            return None
        direction=self.raw.update(landmarks,now,aspect)
        if direction is None:
            self.reason=self.raw.reason
            return None
        if self._at_target(direction,x):
            return self._accept(direction,now)
        self.pending=direction
        self.pending_since=now
        self.reason='继续向'+('左' if direction=='left' else '右')+'越过标记线'
        return None

    def diagnostics(self,now):
        raw=self.raw.diagnostics(now)
        raw['raw_reading_armed']=raw['reading_armed']
        raw['reading_armed']=self.phase in ('ready','outbound')
        return dict(raw,navigation_phase=self.phase,
                    navigation_origin_x=self.origin,
                    navigation_home_half_width=self.HOME_HALF_WIDTH,
                    navigation_stroke_distance=self.STROKE_DISTANCE,
                    pending_direction=self.pending,
                    accepted_cooldown_remaining=round(max(0.,COOLDOWN_SECONDS-(now-self.last_accepted)),3))
