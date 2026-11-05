"""One outward command per confirmed home/return cycle."""
import math
import statistics
from collections import deque
from reading_swipe import ReadingSwipeDetector, COOLDOWN_SECONDS, palm_sample_info


class ReadingNavigationController:
    CENTER_LEFT=.2
    CENTER_RIGHT=.8
    HOME_HALF_WIDTH=.06
    STROKE_DISTANCE=.10
    HOME_MEMORY_SECONDS=3.
    HOME_STILL_SECONDS=.18
    HOME_STILL_RANGE=.025
    SMALL_CONFIRM_SECONDS=2.8
    READY_GAP_SECONDS=1.
    RECOVERY_STILL_SECONDS=.6

    def __init__(self):
        self.raw=ReadingSwipeDetector(minimum_distance=.05,vertical_tolerance=.03,vertical_ratio=.8)
        self.phase='await_center'
        self.origin=.5
        self.home_y=.4
        self.home_known=False
        self.home_span=None
        self.home_size=None
        self.outbound_direction=None
        self.stroke_distance=self.STROKE_DISTANCE
        self.last_time=None
        self.last_valid_at=None
        self.last_full_hand_at=None
        self.edge_continuation=False
        self.home_observations=[]
        self.previous=None
        self.last_accepted=-math.inf
        self.last_direction=None
        self.outward_peak=0.
        self.return_observed=False
        self.pending=None
        self.pending_since=None
        self.candidate=None
        self.neutral_samples=deque(maxlen=32)
        self.center_samples=deque(maxlen=3)
        self.accepted_counts={'left':0,'right':0}
        self.event_id=0
        self.reason='请张开手掌，在起手位置稍停'

    def _clear_motion(self):
        self.raw=ReadingSwipeDetector(minimum_distance=.05,vertical_tolerance=.03,vertical_ratio=.8)
        self.raw.last_trigger=self.last_accepted
        self.pending=self.pending_since=None
        self.candidate=None
        self.outbound_direction=None
        self.center_samples.clear()

    def _prime(self,observations,aspect):
        self._clear_motion()
        self.raw.prime([(landmarks,now) for landmarks,now,_,_ in observations],aspect)
        self.raw.last_trigger=self.last_accepted

    def _home_region(self,x):
        if not self.CENTER_LEFT<=x<=self.CENTER_RIGHT:return False
        if not self.home_known:return True
        if self.phase=='recover_home':
            # Reacquisition must not require hitting the old half-stroke box
            # indefinitely. Nearby positions rearm normally; a shifted central
            # home needs a longer explicit stationary confirmation.
            return abs(x-self.origin)<=self.HOME_HALF_WIDTH+1e-8 or .3<=x<=.7
        if self.phase=='returning':
            # A stop at the counted outer endpoint must not rearm. A stable
            # return endpoint may lie a little beyond the old home position.
            # A large stroke often returns to a slightly shifted natural home.
            # Require at least 65% of the observed excursion to be recovered,
            # rather than an invariant six-percent box. Short strokes keep a
            # tight return band, so pausing halfway back cannot rearm them.
            radius=min(.12,max(.02,self.outward_peak*.35+(.005 if self.outward_peak>=.2 else 0.)))
            if self.last_direction=='left':
                return self.origin-radius<=x<=self.origin+.18
            return self.origin-.18<=x<=self.origin+radius
        return abs(x-self.origin)<=self._home_radius()+1e-8

    def _home_radius(self):
        return min(self.HOME_HALF_WIDTH,self.stroke_distance*.5)

    def _turn_home_region(self,x):
        if not self._home_region(x):return False
        # A continuous return overshoot followed by moving back toward home
        # is still the return, not a new outward stroke.
        if self.last_direction=='left':return x<=self.origin+self.HOME_HALF_WIDTH
        return x>=self.origin-self.HOME_HALF_WIDTH

    @staticmethod
    def _palm_size(points,aspect):
        # x and z are normalized by frame width; y by frame height. Compare
        # palm breadth in consistent 3-D units, not its shrinking x projection
        # when the hand rotates during a swipe.
        a,b=points[5],points[17]
        return math.sqrt((a.x-b.x)**2+((a.y-b.y)*aspect)**2+(a.z-b.z)**2)

    def _calibrate_distance(self,observations,aspect=.75):
        spans=[max(p[i].x for i in (5,9,13,17))-min(p[i].x for i in (5,9,13,17))
               for p,_,_,_ in observations]
        if spans:
            # About a third of a visible palm width, bounded in frame coordinates so
            # distant hands do not become jitter triggers or require a very
            # large excursion. Lock this threshold for the outward cycle.
            self.home_span=statistics.median(spans)
            self.home_size=statistics.median(self._palm_size(p,aspect) for p,_,_,_ in observations)
            self.stroke_distance=max(.05,min(.10,self.home_span*.32))

    def _sparse_endpoint(self,landmarks,previous,now,x,y,aspect):
        """A sparse but observed stroke from a confirmed home is not a track jump.

        Palm detection may take longer than tracking, leaving a large horizontal
        step between adjacent captured frames. Only bridge that step within an
        armed outward cycle, with recent input and a consistent visible palm.
        Return and reacquisition phases never call this fallback.
        """
        if previous is None or not self.home_known or not self.home_size:
            return None
        if not .15<=now-previous[1]<=.5:return None
        dx=x-self.origin
        if not self.stroke_distance<=abs(dx)<=.45:return None
        direction='left' if dx<0 else 'right'
        if direction!=self.outbound_direction:return None
        if not 2/3<=self._palm_size(landmarks,aspect)/self.home_size<=1.5:return None
        if abs(y-self.home_y)>max(.03,abs(dx)*.8):return None
        return direction

    def _neutral(self,observation):
        _,now,x,y=observation
        if not self._home_region(x):
            self.neutral_samples.clear();return False
        samples=list(self.neutral_samples)
        if samples and (now-samples[-1][1]>.5
                or max([x]+[p[2] for p in samples])-min([x]+[p[2] for p in samples])>self.HOME_STILL_RANGE
                or max([y]+[p[3] for p in samples])-min([y]+[p[3] for p in samples])>.03):
            self.neutral_samples.clear()
        self.neutral_samples.append(observation)
        required=self.HOME_STILL_SECONDS
        if (self.phase=='recover_home' and self.home_known
                and abs(x-self.origin)>self.HOME_HALF_WIDTH):
            required=self.RECOVERY_STILL_SECONDS
        return (len(self.neutral_samples)>=2
                and now-self.neutral_samples[0][1]>=required-1e-8)

    def _ready(self,aspect):
        samples=list(self.neutral_samples)
        self.origin=sum(p[2] for p in samples)/len(samples)
        self.home_y=sum(p[3] for p in samples)/len(samples)
        self.home_known=True
        self._calibrate_distance(samples,aspect)
        self._prime(samples[-3:],aspect)
        self.center_samples.extend(samples[-3:])
        self.home_observations=samples[-3:]
        self.phase='ready'
        self.neutral_samples.clear()
        self.reason='已就绪，可以向左或向右挥动'

    def _accept(self,direction,now,x,confirmed_small=False,observed_peak=0.):
        if now-self.last_accepted<COOLDOWN_SECONDS:
            self.reason='短暂冷却中，请回到起手位置';return None
        if not confirmed_small and abs(x-self.origin)<max(.10,self.stroke_distance*1.6):
            self.candidate=dict(direction=direction,started=now,peak=abs(x-self.origin))
            self.phase='confirming';self.pending=self.pending_since=None
            self.reason='小幅'+('左' if direction=='left' else '右')+'挥待确认，请继续外挥或回位'
            return None
        self.last_accepted=now;self.last_direction=direction
        self.outward_peak=max(abs(x-self.origin),observed_peak)
        self.return_observed=False
        self.candidate=None
        self.raw.last_trigger=now
        self.accepted_counts[direction]+=1;self.event_id+=1
        self.phase='returning';self.pending=self.pending_since=None
        self.center_samples.clear();self.neutral_samples.clear()
        self.reason=('左挥' if direction=='left' else '右挥')+'已计数，请回到起手位置'
        return direction

    def _confirm_candidate(self,landmarks,sample,reason,now,aspect):
        candidate=self.candidate
        if now-candidate['started']>self.SMALL_CONFIRM_SECONDS:
            self._clear_motion();self.phase='recover_home';self.previous=None
            self.reason='小幅动作未完成，已取消，请回位';return None
        if sample is None or sample[2]<140:
            self.previous=None
            if landmarks is not None:
                # A visible closed, distorted or deeply clipped hand is an
                # explicit cancellation. A brief detector miss may wait only
                # for this already observed candidate's bounded return check.
                self._clear_motion();self.phase='recover_home'
                self.reason=reason or '收手已取消小幅动作，请回位'
            else:self.reason='小幅动作待确认，等待手掌回位'
            return None
        x,y,_=sample
        observation=(landmarks,now,x,y)
        recent=self.previous is not None and 0<now-self.previous[1]<=.5
        self.previous=observation;self.last_valid_at=now
        size=self._palm_size(landmarks,aspect)
        same_size=self.home_size is not None and self.home_size>0 and 2/3<=size/self.home_size<=1.5
        direction=candidate['direction'];sign=-1 if direction=='left' else 1
        excursion=sign*(x-self.origin)
        candidate['peak']=max(candidate['peak'],excursion)
        if (excursion>=max(.10,self.stroke_distance*1.6)
                and abs(y-self.home_y)<=max(.03,abs(x-self.origin)*.8)
                and ((recent and not self.edge_continuation) or same_size)):
            # A continuously visible open hand can rotate during a stroke;
            # projected horizontal palm span is not an identity invariant.
            return self._accept(direction,now,x,True,candidate['peak'])
        if not same_size:
            self._clear_motion();self.phase='recover_home'
            self.reason='手掌尺寸变化，已取消小幅动作';return None
        radius=min(self.HOME_HALF_WIDTH,candidate['peak']*.35)
        if (-self.HOME_HALF_WIDTH<=excursion<=radius
                and abs(y-self.home_y)<=.06):
            result=self._accept(direction,now,x,True,candidate['peak'])
            if result:
                self.return_observed=True
                self.neutral_samples.append(observation)
            return result
        self.reason='小幅'+('左' if direction=='left' else '右')+'挥待确认，请继续外挥或回位'
        return None

    def update(self,landmarks,now,aspect=.75):
        if not math.isfinite(now) or (self.last_time is not None and now<=self.last_time):return None
        gap=self.last_time is not None and now-self.last_time>.5
        self.last_time=now
        sample,reason=palm_sample_info(landmarks,aspect)
        self.edge_continuation=False
        if sample is not None and sample[2]>=140:
            self.last_full_hand_at=now
        elif (sample is None and self.phase in ('ready','outbound','confirming')
                and self.previous is not None and self.last_full_hand_at is not None
                and 0<now-self.last_full_hand_at<=.35 and not gap):
            continued,_=palm_sample_info(landmarks,aspect,allow_edge=True)
            if (continued is not None and continued[2]>=140
                    and (continued[0]<.12 or continued[0]>.88)):
                sample=continued;self.edge_continuation=True
        if self.phase=='confirming':
            return self._confirm_candidate(landmarks,sample,reason,now,aspect)
        if sample is None or sample[2]<140:
            self._clear_motion();self.neutral_samples.clear();self.previous=None
            if self.last_valid_at is not None and now-self.last_valid_at>=self.HOME_MEMORY_SECONDS:
                self.home_known=False
            # Hand loss cancels an unfinished trajectory, not an accepted
            # gesture's return lock. Reacquisition never emits an action.
            if self.phase!='returning':
                self.phase='recover_home' if self.home_known else 'await_center'
            self.reason=reason or '请张开手掌，回到起手位置稍停'
            return None
        x,y,_=sample;observation=(landmarks,now,x,y)
        prior=self.previous
        previous=None if gap else self.previous
        self.previous=observation;self.last_valid_at=now
        if (gap and self.phase=='ready' and self.home_known and prior is not None
                and len(self.center_samples)>=2 and not self.edge_continuation
                and 0<now-prior[1]<=self.READY_GAP_SECONDS
                and abs(prior[2]-self.origin)<self.stroke_distance+1e-8
                and max(.10,self.stroke_distance*1.6)<=abs(x-self.origin)<=.45
                and abs(y-self.home_y)<=max(.03,abs(x-self.origin)*.8)):
            # A slow inference pass is not a missing/invalid hand. The previous
            # valid input was at an already confirmed home and this fresh full
            # open hand is a clear outward endpoint. Keep the return lock for
            # every other phase; invalid inputs never take this path.
            return self._accept('left' if x<self.origin else 'right',now,x)
        if self.phase=='returning':
            excursion=self.origin-x if self.last_direction=='left' else x-self.origin
            self.outward_peak=max(self.outward_peak,excursion)
            if self._home_region(x):self.return_observed=True
        if gap:
            self._clear_motion();self.neutral_samples.clear()
            if self.phase!='returning':self.phase='recover_home' if self.home_known else 'await_center'
        if self.phase in ('await_center','recover_home','returning'):
            if self._neutral(observation):
                self._ready(aspect);return None
            # Repeating the same outward direction can turn around at home
            # without a hold. Merely crossing home on the return cannot rearm
            # an opposite stroke; that needs a confirmed neutral stop.
            same_turn=(self.phase=='returning' and previous is not None
                and self.return_observed and self._turn_home_region(previous[2])
                and abs(y-previous[3])<=max(.03,abs(x-previous[2])*.8)
                and ((self.last_direction=='left' and x<=previous[2]-self.stroke_distance+1e-8)
                     or (self.last_direction=='right' and x>=previous[2]+self.stroke_distance-1e-8)))
            if not same_turn:
                if self.phase=='returning':self.reason='回位中，回程不计数；起手位置稍停后继续'
                elif not self._home_region(x):self.reason='请把手掌移到画面中部，再稍停'
                elif self.phase=='recover_home':self.reason='正在重新确认起点，请停稳约半秒'
                else:self.reason='请在起手位置稍停，等待已就绪'
                return None
            self.origin=previous[2];self.home_y=previous[3];self.home_known=True
            self._calibrate_distance([previous],aspect)
            self._prime([previous],aspect);self.phase='outbound';self.neutral_samples.clear()
            self.outbound_direction=self.last_direction
        elif self.phase=='ready':
            # Small resting drift must not commit an outward direction before
            # the minimum stroke distance is reached. Otherwise a slight wobble
            # can poison the subsequent real stroke's primed trajectory.
            if abs(x-self.origin)<self.stroke_distance-1e-8:
                self.center_samples.append(observation)
                samples=list(self.center_samples)
                self._prime(samples,aspect)
                self.center_samples.extend(samples)
                self.home_observations=samples[-3:]
                self.reason='已就绪，可以向左或向右挥动'
                return None
            samples=list(self.center_samples)
            # Trim neutral jitter that points against the new outward
            # direction, as the original recognizer does. Otherwise a small
            # resting oscillation makes a real stroke look like a reversal.
            sign=-1 if x<self.origin else 1
            index=len(samples)-1
            while index>0 and sign*(samples[index][2]-samples[index-1][2])>=-.01:
                index-=1
            self._prime(samples[max(0,index):],aspect);self.phase='outbound'
            self.outbound_direction='left' if sign<0 else 'right'
        elif (self.phase=='outbound' and previous is not None
                and abs(x-self.origin)<=self._home_radius()+1e-8
                and abs(x-self.origin)<=abs(previous[2]-self.origin)+1e-8):
            # An uncounted wobble returning to home can supply its own recent
            # neutral pair. Do not discard that pair and wait at a stale origin.
            self._clear_motion();self.phase='recover_home';self.neutral_samples.clear()
            if abs(previous[2]-self.origin)<=self.HOME_HALF_WIDTH+1e-8:
                self._neutral(previous)
            if self._neutral(observation):self._ready(aspect)
            else:self.reason='动作未完成，正在重新确认起点，请稍停'
            return None
        if self.pending is not None:
            if now-self.pending_since>1.8:
                self._clear_motion();self.phase='recover_home'
                self.reason='动作已超时，请回到起手位置';return None
            direction=self.pending
        else:
            direction=self.raw.update(landmarks,now,aspect)
            if direction is None and (self.raw.gate=='collecting' or self.edge_continuation):
                direction=self._sparse_endpoint(landmarks,previous,now,x,y,aspect)
            if direction is None:
                self.reason=self.raw.reason;return None
            self.pending=direction;self.pending_since=now
        target=(x<=self.origin-self.stroke_distance+1e-8 if direction=='left'
                else x>=self.origin+self.stroke_distance-1e-8)
        if target and abs(y-self.home_y)<=max(.03,abs(x-self.origin)*.8):
            return self._accept(direction,now,x)
        self.reason='继续向'+('左' if direction=='left' else '右')+'挥动'
        return None

    def diagnostics(self,now):
        raw=self.raw.diagnostics(now)
        raw['raw_reading_armed']=raw['reading_armed']
        raw['reading_armed']=self.phase in ('ready','outbound','confirming')
        return dict(raw,navigation_phase=self.phase,navigation_origin_x=self.origin,
            navigation_home_known=self.home_known,navigation_home_half_width=self._home_radius(),
            navigation_stroke_distance=self.stroke_distance,pending_direction=self.pending,
            neutral_confirmed_seconds=(now-self.neutral_samples[0][1] if self.neutral_samples else 0.),
            recognition_event_id=self.event_id,recognition_counts=dict(self.accepted_counts),
            edge_continuation=self.edge_continuation,
            provisional_direction=self.candidate['direction'] if self.candidate else None,
            accepted_cooldown_remaining=round(max(0.,COOLDOWN_SECONDS-(now-self.last_accepted)),3))
