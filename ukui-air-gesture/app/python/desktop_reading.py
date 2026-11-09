"""Deliberate open-palm -> held-fist return to desktop for reading mode.

The legacy classifier's fist screenshot is unchanged. Desktop delivery is a
bounded Wayland minimize request, never a per-frame toggle or an automatic retry.
"""
import asyncio
import math
import subprocess
import time

from reading_freshness import MAX_RESULT_AGE_SECONDS, result_is_fresh


class DesktopDBusActions:
    """Compatibility name for an idempotent native Wayland desktop action.

    The Bianbu panel's toggle can retain its old state when UNO explicitly
    restores an Impress window. Minimize requests have no such toggle state.
    """
    COMMAND = '/usr/bin/wlrctl'

    def show_desktop(self):
        try:
            result = subprocess.run([self.COMMAND, 'toplevel', 'minimize', 'state:unminimized'],
                                    text=True, capture_output=True, timeout=2.)
            # Like find, minimize exits 1 with no stderr when no window
            # matches (the desktop is already showing). Still verify below.
            if result.returncode not in (0, 1) or result.stderr.strip():
                raise RuntimeError('Wayland minimize failed: '+result.stderr.strip()[:300])
            deadline = time.monotonic()+2.
            while time.monotonic() < deadline:
                status = subprocess.run([self.COMMAND, 'toplevel', 'find', 'state:unminimized'],
                                        text=True, capture_output=True, timeout=1.)
                # wlrctl find: 0 means at least one match, 1 means no match.
                # A protocol failure also emits stderr and is never success.
                if status.returncode == 1 and not status.stderr.strip():
                    return True
                if status.returncode not in (0, 1) or status.stderr.strip():
                    raise RuntimeError('Wayland window state unavailable: '+status.stderr.strip()[:300])
                time.sleep(.05)
        except (OSError, subprocess.TimeoutExpired) as error:
            raise RuntimeError('Wayland desktop action failed: '+type(error).__name__) from error
        raise RuntimeError('Wayland desktop action did not confirm all windows minimized')

    async def _show_desktop(self):
        return await asyncio.to_thread(self.show_desktop)

    def close(self):
        # No persistent D-Bus connection, event loop or child process is owned.
        pass


class HeldFistDetector:
    OPEN_SECONDS = .45
    HOLD_SECONDS = 1.2
    HOLD_RELEASE_OPENNESS = 125.
    HOLD_RELEASE_FINGER_ANGLE = 145.
    MAX_GAP = .9
    MAX_OPEN_GAP = MAX_RESULT_AGE_SECONDS+.2
    MAX_MOVEMENT = .045
    FINGER_MARGIN = .03
    TRANSITION_SECONDS = 2.

    def __init__(self):
        self.last_time = None
        self.metrics = {}
        self.previous_metrics = {}
        self.last_valid_metrics = {}
        self.reset_count = 0
        self.last_reset_reason = 'initial'
        self.last_reset_metrics = {}
        self._clear_track()

    def _clear_track(self):
        self.open_since = self.closed_since = None
        self.last_open_time = self.transition_since = None
        self.last_was_valid_open = False
        self.transition_errors = 0
        self.transition_error = ''
        self.anchor = self.palm_anchor = self.wrist_anchor = None
        self.armed = False
        self.samples = self.soft_samples = 0
        self.arming_seconds = 0.
        self.phase = self.gate = 'await_open'
        self.reason = ''

    def _remember_reset(self, reason):
        # Do not overwrite the last interrupted attempt with empty invalid
        # frames. Logs sampled seconds later must retain its actual cause.
        if self.open_since is not None or self.armed or self.closed_since is not None:
            self.reset_count += 1
            self.last_reset_reason = reason
            self.last_reset_metrics = dict(self.metrics)
            previous = False
            for key in ('openness','fingers_closed','anchor_distance','palm_distance','wrist_distance',
                        'palm_size','palm_x_span','index_angle','middle_angle','ring_angle','pinky_angle','max_finger_angle',
                        'open_visible','open_fully_visible','open_visible_fingers','open_border_mask','open_frame_margin','fist_qualification'):
                if self.last_reset_metrics.get(key) is None and self.last_valid_metrics.get(key) is not None:
                    self.last_reset_metrics[key] = self.last_valid_metrics[key]
                    previous = True
            self.last_reset_metrics['used_previous_geometry'] = previous

    def reset(self, reason='tracking_reset'):
        self._remember_reset(reason)
        self._clear_track()

    @staticmethod
    def finger_angles(landmarks, aspect):
        def angle(a, b, c):
            va = (a.x-b.x, (a.y-b.y)*aspect, a.z-b.z)
            vb = (c.x-b.x, (c.y-b.y)*aspect, c.z-b.z)
            norm = math.sqrt(sum(v*v for v in va)*sum(v*v for v in vb))
            if norm < 1e-10:
                return 180.
            return math.degrees(math.acos(max(-1., min(1., sum(x*y for x, y in zip(va, vb))/norm))))
        # Do not treat three curled fingers plus a pointing finger as a fist.
        return tuple((angle(*landmarks[m:m+3])+angle(*landmarks[m+1:m+4]))/2
                     for m in (5, 9, 13, 17))

    @classmethod
    def fingers_closed(cls, landmarks, aspect):
        return all(value <= 135 for value in cls.finger_angles(landmarks,aspect))

    @classmethod
    def sample(cls, landmarks, aspect):
        """Geometry for a central fist, independent of open-palm swipe gates."""
        if landmarks is None or len(landmarks) != 21:
            return None, 'missing_landmarks'
        if not math.isfinite(aspect) or aspect <= 0:
            return None, 'invalid_aspect'
        points = [(float(p.x), float(p.y), float(p.z)) for p in landmarks]
        if not all(math.isfinite(value) for point in points for value in point):
            return None, 'nonfinite_landmarks'
        if not all(.02 <= points[0][axis] <= .98 for axis in (0, 1)):
            return None, 'wrist_not_visible'
        ids = (5, 9, 13, 17)
        if not all(.02 <= points[i][axis] <= .98 for i in ids for axis in (0, 1)):
            return None, 'palm_not_visible'
        span = max(points[i][0] for i in ids)-min(points[i][0] for i in ids)
        palm = (1-sum(points[i][0] for i in ids)/4,
                aspect*sum(points[i][1] for i in ids)/4)
        wrist = (1-points[0][0], aspect*points[0][1])
        anchor = ((palm[0]+wrist[0])/2, (palm[1]+wrist[1])/2)
        core = (0,)+ids
        size = max(math.hypot(points[a][0]-points[b][0],
                              (points[a][1]-points[b][1])*aspect)
                   for a in core for b in core)
        # Closing/turning the hand can narrow its x-span while the wrist and
        # knuckles still describe a full palm. Use their stable 2D body size.
        if not .04 <= size <= .85:
            return None, 'palm_size'
        # Folded/occluded joint predictions need not satisfy the extended-hand
        # visibility gate. Keep their frame margin small AND their geometry
        # bounded by the visible palm, so arbitrary off-screen points fail.
        for m in ids:
            for i in range(m+1, m+4):
                if (not all(-cls.FINGER_MARGIN <= points[i][axis] <= 1+cls.FINGER_MARGIN for axis in (0, 1))
                    or math.hypot(points[i][0]-points[m][0],
                                  (points[i][1]-points[m][1])*aspect) > 2*size):
                    return None, 'finger_geometry'
        def angle(a, b, c):
            va = tuple(points[a][i]-points[b][i] for i in range(3))
            vb = tuple(points[c][i]-points[b][i] for i in range(3))
            norm = math.sqrt(sum(v*v for v in va)*sum(v*v for v in vb))
            if norm < 1e-10:
                return 0.
            return math.degrees(math.acos(max(-1., min(1., sum(x*y for x, y in zip(va, vb))/norm))))
        openness = sum((angle(m,m+1,m+2)+angle(m+1,m+2,m+3))/2 for m in ids)/4
        visible = tuple(all(.01 <= points[i][axis] <= .99
                            for i in range(m,m+4) for axis in (0,1)) for m in ids)
        visible_fingers = sum(visible)
        # Preparation agrees with the swipe's three-visible-finger gate.
        # All four fingers still passed the bounded geometry checks above;
        # this allowance never changes the four-closed-finger hold rule.
        open_visible = visible_fingers >= 3
        frame_margin = min(min(points[i][axis],1-points[i][axis])
                           for i in range(5,21) for axis in (0,1))
        angles=cls.finger_angles(landmarks,aspect)
        return dict(palm=palm, wrist=wrist, anchor=anchor, openness=openness,
                    fingers_closed=all(value <= 135 for value in angles),
                    open_visible=open_visible, open_fully_visible=all(visible),
                    open_visible_fingers=visible_fingers,
                    open_border_mask=sum(1<<i for i,value in enumerate(visible) if not value),
                    open_frame_margin=frame_margin, palm_size=size, palm_x_span=span,
                    index_angle=angles[0], middle_angle=angles[1],
                    ring_angle=angles[2], pinky_angle=angles[3],
                    max_finger_angle=max(angles)), None

    def _set_anchor(self, sample):
        self.anchor, self.palm_anchor, self.wrist_anchor = (sample[key] for key in ('anchor','palm','wrist'))
        self.metrics.update(anchor_distance=0.,palm_distance=0.,wrist_distance=0.)

    def _transition_expired(self, timestamp):
        if self.transition_since is None:
            self.transition_since = self.last_open_time if self.last_open_time is not None else timestamp
        self.metrics['transition_seconds'] = timestamp-self.transition_since
        return timestamp-self.transition_since >= self.TRANSITION_SECONDS-1e-8

    def update(self, landmarks, timestamp, aspect=.75, age=0.):
        if not math.isfinite(timestamp) or (self.last_time is not None and timestamp <= self.last_time):
            self.gate = 'invalid_timestamp'
            self.last_was_valid_open = False
            return False
        gap = timestamp-self.last_time if self.last_time is not None else 0.
        self.last_time = timestamp
        self.previous_metrics = self.metrics
        self.metrics = dict(result_age_seconds=age if math.isfinite(age) else None,
                            frame_gap_seconds=gap, openness=None, fingers_closed=None,
                            anchor_distance=None, palm_distance=None, wrist_distance=None,
                            palm_size=None,palm_x_span=None,index_angle=None,middle_angle=None,
                            ring_angle=None,pinky_angle=None,max_finger_angle=None,
                            open_visible=None,open_fully_visible=None,open_visible_fingers=None,
                            open_border_mask=None,open_frame_margin=None,open_gap_preserved=False,
                            arming_seconds=self.arming_seconds,transition_seconds=0.,
                            hold_seconds=timestamp-self.closed_since if self.closed_since is not None else 0.,
                            hold_samples=self.samples,hold_strict_samples=self.samples,
                            hold_soft_samples=self.soft_samples,fist_qualification=None)
        was_open = self.last_was_valid_open
        self.last_was_valid_open = False
        fresh = result_is_fresh(age)
        sample,error = self.sample(landmarks,aspect) if fresh else (None,'stale_result')
        current_open = (sample is not None and sample['openness'] >= 155 and sample['open_visible']
                        and .25 <= sample['palm'][0] <= .75
                        and .15*aspect <= sample['palm'][1] <= .85*aspect)
        preserve_open = (self.MAX_GAP < gap <= self.MAX_OPEN_GAP and was_open
                         and self.phase == 'opening' and not self.armed
                         and self.closed_since is None and self.transition_since is None
                         and result_is_fresh(self.previous_metrics.get('result_age_seconds',math.inf))
                         and fresh and current_open)
        # A normal first full detector pass can approach the 1 s freshness
        # budget. Preserve only two verified open endpoints during startup;
        # armed, closing and holding keep the original .9 s gap limit.
        if gap > self.MAX_GAP and not preserve_open:
            self.reset('frame_gap')
        self.metrics['open_gap_preserved'] = preserve_open
        if not fresh:
            self.reset('stale_result')
            self.gate = 'stale_result'
            self.reason = '握拳采样已过期，请重新张开手掌停稳'
            return False
        if sample is None:
            # A brief geometric wobble while closing is not a failed hold:
            # no hold has started yet. Hand loss, stale data and a bad held
            # fist still reset immediately. No invalid sample can start one.
            if self.armed and self.closed_since is None and error not in ('missing_landmarks','invalid_aspect'):
                self.transition_errors += 1
                self.transition_error = error
                if not self._transition_expired(timestamp):
                    self.phase = 'closing'
                    self.gate = 'transition_'+error
                    self.reason = '正在识别握拳过渡，请保持手腕在中央'
                    return False
                error = 'transition_timeout'
            self.reset(error)
            self.gate = error
            self.reason = '请让手腕和手掌完整入镜，再张开手掌停稳'
            return False
        x,y = sample['palm']
        openness = sample['openness']
        self.metrics.update((key,sample[key]) for key in
                            ('openness','fingers_closed','palm_size','palm_x_span','index_angle',
                             'middle_angle','ring_angle','pinky_angle','max_finger_angle',
                             'open_visible','open_fully_visible','open_visible_fingers','open_border_mask','open_frame_margin'))
        for key,current,anchor in (('anchor_distance',sample['anchor'],self.anchor),
                                   ('palm_distance',sample['palm'],self.palm_anchor),
                                   ('wrist_distance',sample['wrist'],self.wrist_anchor)):
            if anchor is not None:
                self.metrics[key] = math.hypot(current[0]-anchor[0],current[1]-anchor[1])
        strict = openness <= 115 and sample['fingers_closed']
        within_release = (openness <= self.HOLD_RELEASE_OPENNESS
                          and sample['max_finger_angle'] <= self.HOLD_RELEASE_FINGER_ANGLE)
        self.metrics['fist_qualification'] = 'strict' if strict else 'soft' if within_release else 'open'
        self.last_valid_metrics = self.metrics
        if not .25 <= x <= .75 or not .15*aspect <= y <= .85*aspect:
            self.reset('off_center')
            self.gate = 'off_center'
            self.reason = '请在画面中央重新张开手掌停稳'
            return False
        # Only an actual closed-fist hold has a fixed motion anchor. The
        # observed user's open hand moved naturally while entering position;
        # accumulating that movement must not invalidate a later stable fist.
        distance = self.metrics['anchor_distance']
        if self.closed_since is not None and distance is not None and distance > self.MAX_MOVEMENT:
            self.reset('anchor_movement')
            self.gate = 'anchor_movement'
            self.reason = '握拳动作因手部移动取消，请重新张开手掌停稳'
            return False
        if openness >= 155 and sample['open_visible']:
            if self.closed_since is not None:
                self._remember_reset('fist_reopened')
            if self.open_since is None:
                self.open_since = timestamp
            self.closed_since = None
            self.samples = self.soft_samples = 0
            self.last_open_time = timestamp
            self.last_was_valid_open = True
            self.transition_since = None
            self.transition_errors = 0
            self.transition_error = ''
            self._set_anchor(sample)  # Rolling last-open reference, not a hold.
            self.arming_seconds = timestamp-self.open_since
            self.metrics.update(arming_seconds=self.arming_seconds,hold_seconds=0.,hold_samples=0,hold_strict_samples=0,hold_soft_samples=0)
            self.armed = self.arming_seconds >= self.OPEN_SECONDS-1e-8
            self.phase = self.gate = 'armed' if self.armed else 'opening'
            self.reason = ''
            return False
        self.open_since = None
        if not self.armed:
            self.phase = 'latched' if self.phase == 'latched' else 'await_open'
            self.gate = self.phase
            return False
        if self.closed_since is None and self._transition_expired(timestamp):
            self.reset('transition_timeout')
            self.gate = 'transition_timeout'
            self.reason = '握拳过渡时间过长，请重新张开手掌准备'
            return False
        if not strict:
            # Hysteresis applies only to an already established hold, after
            # the unarmed/latched guard above. Near-threshold frames preserve
            # the fixed anchor and elapsed hold, never strict evidence.
            if self.closed_since is not None and within_release:
                self.soft_samples += 1
                self.metrics.update(hold_seconds=timestamp-self.closed_since,
                                    hold_samples=self.samples,hold_strict_samples=self.samples,
                                    hold_soft_samples=self.soft_samples)
                self.phase = 'holding'
                self.gate = 'holding_soft'
                self.reason = '请保持握拳，正在确认；张开手可取消'
                return False
            if self.closed_since is not None:
                self._remember_reset('fist_incomplete')
                self.transition_since = timestamp
            self.closed_since = None
            self.samples = self.soft_samples = 0
            self.metrics.update(hold_seconds=0.,hold_samples=0,hold_strict_samples=0,hold_soft_samples=0)
            self.phase = self.gate = 'closing'
            self.reason = '请收拢四指握拳，保持手腕位置稳定'
            return False
        if self.closed_since is None:
            self.closed_since = timestamp
            self.soft_samples = 0
            self._set_anchor(sample)  # First valid closed fist starts the hold.
        self.samples += 1
        held = timestamp-self.closed_since
        self.metrics.update(hold_seconds=held,hold_samples=self.samples,
                            hold_strict_samples=self.samples,hold_soft_samples=self.soft_samples)
        remaining = max(0., self.HOLD_SECONDS-held)
        self.phase = self.gate = 'holding'
        self.reason = ('握拳保持，约'+format(remaining, '.1f')+'秒后返回桌面；张开手可取消'
                       if remaining > 1e-8 else '请保持握拳，正在确认；张开手可取消')
        if remaining > 1e-8 or self.samples < 4:
            return False
        self.armed = False
        self.phase = self.gate = 'latched'
        self.reason = '返回桌面手势已识别，请张开手后再操作'
        return True

    def preparation_feedback(self):
        if self.phase == 'armed':
            return '握拳返回桌面已就绪，握拳保持约1.2秒'
        if self.phase == 'opening':
            return '握拳准备'+format(self.arming_seconds,'.2f')+'/0.45秒，请保持张掌'
        if self.phase in ('closing','holding','latched'):
            return self.reason
        if self.metrics.get('open_visible') is False and (self.metrics.get('openness') or 0) >= 155:
            count = self.metrics.get('open_visible_fingers',0)
            return '握拳未就绪：完整可见手指'+str(count)+'/3，请将手指移入画面'
        if self.gate in ('wrist_not_visible','palm_not_visible'):
            return '握拳未就绪：请让手腕和掌部完整入镜'
        if self.gate in ('palm_size','finger_geometry'):
            return '握拳未就绪：请调整手与镜头距离，让指节完整入镜'
        return '握拳尚未准备，请先张开手掌等到就绪'

    def diagnostics(self):
        def scalar(value):
            return round(value,4) if isinstance(value,float) and math.isfinite(value) else value
        values = dict(desktop_gesture_phase=self.phase,desktop_decision_gate=self.gate,
                      desktop_reset_reason=self.last_reset_reason,desktop_reset_count=self.reset_count,
                      desktop_anchor_source='wrist_mcp_midpoint',
                      desktop_prepared=self.armed,
                      desktop_preparation_feedback=self.preparation_feedback(),
                      desktop_anchor_role='holding' if self.closed_since is not None else 'open_reference' if self.anchor is not None else 'none',
                      desktop_transition_invalid_count=self.transition_errors,
                      desktop_transition_invalid_reason=self.transition_error)
        values.update(('desktop_'+key,scalar(value)) for key,value in self.metrics.items())
        values.update(('desktop_last_reset_'+key,scalar(value))
                      for key,value in self.last_reset_metrics.items())
        return values


def session_identity(state):
    owner = state.get('reader_owner')
    if not isinstance(owner, str) or not owner.startswith(':'):
        raise RuntimeError('Reading session owner is unavailable')
    return owner, state.get('session_id')


class DesktopReadingDispatcher:
    """Wrap normal navigation; pause hidden slides until an explicit reopen."""
    VISIBILITY_MAX_AGE = 2.

    def __init__(self, navigation, reader, show_desktop, emit, clock=time.monotonic):
        self.clock = clock
        self.pause_completed_at = None
        self.resume_gate = 'not_paused'
        self.navigation = navigation
        self.reader = reader
        self.show_desktop = show_desktop
        self.emit = emit
        self.detector = HeldFistDetector()
        self.paused_identity = None
        self.pause_reason = '已返回桌面；恢复PPT窗口或语音重新打开后继续手势翻页'
        self.last_resume_check = -math.inf

    @property
    def controller(self):
        return self.navigation.controller

    def _visible_after_delivery(self, state):
        if state.get('active') is not True:
            self.resume_gate = 'inactive_presentation'
            return False
        if state.get('presentation_visible') is not True or state.get('visibility_source') != 'wayland_exact_window':
            self.resume_gate = 'visibility_unconfirmed'
            return False
        checked = state.get('visibility_checked_at')
        if type(checked) not in (int,float) or not math.isfinite(checked):
            self.resume_gate = 'invalid_visibility_timestamp'
            return False
        if self.pause_completed_at is None or checked <= self.pause_completed_at:
            self.resume_gate = 'visibility_before_desktop_completed'
            return False
        age = self.clock()-checked
        if not math.isfinite(age) or not 0 <= age <= self.VISIBILITY_MAX_AGE:
            self.resume_gate = 'visibility_stale_or_future'
            return False
        self.resume_gate = 'managed_presentation_visible'
        return True

    def process(self, landmarks, timestamp, aspect=.75, age=0.):
        if self.paused_identity is not None:
            if timestamp-self.last_resume_check >= .5:
                self.last_resume_check = timestamp
                try:
                    state = self.reader.state()
                    new_session = session_identity(state) != self.paused_identity
                    resumed = self._visible_after_delivery(state)
                except Exception:
                    resumed = False
                    self.resume_gate = 'state_unavailable'
                if resumed:
                    self.paused_identity = None
                    self.pause_completed_at = None
                    self.detector.reset('new_reader_session' if new_session else 'managed_presentation_visible')
                    self.navigation.reset_tracking()
                    self.emit('reading_resumed', delivery='new_reader_session' if new_session else 'visible_managed_presentation')
            if self.paused_identity is not None:
                self.controller.reason = self.pause_reason
                return None
        resets = self.detector.reset_count
        triggered = self.detector.update(landmarks, timestamp, aspect, age)
        cancelled = self.detector.reset_count != resets
        if cancelled:
            self.emit('desktop_gesture_cancelled', **self.detector.diagnostics())
        if triggered:
            try:
                state = self.reader.state()
                if not state.get('active'):
                    raise RuntimeError('No active presentation to leave')
                identity = session_identity(state)
                self.navigation.cancel_pending()
                # A lost acknowledgement can follow an executed minimize request. Pause before
                # delivery too; never navigate hidden slides on an ambiguous
                # failure or attempt the toggle again automatically.
                self.paused_identity = identity
                self.pause_reason = '返回桌面结果未确认；恢复PPT窗口或语音重新打开后继续手势翻页'
                self.pause_completed_at = None
                self.resume_gate = 'waiting_post_delivery_visibility'
                try:
                    if self.show_desktop() is not True:
                        raise RuntimeError('Desktop action was not acknowledged')
                finally:
                    # A probe begun before this completed action can still
                    # carry pre-minimize True; it is never a restore signal.
                    self.pause_completed_at = self.clock()
            except Exception as error:
                self.emit('action_error', kind='desktop', error=str(error), automatic_retry=False)
                self.navigation.reset_tracking()
                return None
            self.pause_reason = '已返回桌面；恢复PPT窗口或语音重新打开后继续手势翻页'
            self.navigation.reset_tracking()
            self.emit('desktop_requested', gesture='open_then_hold_fist', delivery='wayland_minimized_verified')
            self.controller.reason = '已返回桌面；恢复PPT窗口或语音重新打开后继续手势翻页'
            return None
        result = self.navigation.process(landmarks, timestamp, aspect, age=age)
        if (self.detector.phase in ('closing','holding')
            or cancelled and self.detector.metrics.get('fingers_closed')):
            self.controller.reason = self.detector.reason
        elif landmarks is not None:
            self.controller.reason = self.detector.preparation_feedback()+'；翻页：'+self.controller.reason
        return result

    def diagnostics(self):
        state = self.navigation.diagnostics() if hasattr(self.navigation, 'diagnostics') else {}
        return dict(state, **self.detector.diagnostics(),
                    reading_paused_for_desktop=self.paused_identity is not None,
                    reading_pause_completed_at=self.pause_completed_at,
                    reading_resume_gate=self.resume_gate)

    def close(self):
        if hasattr(self.navigation, 'close'):
            self.navigation.close()
