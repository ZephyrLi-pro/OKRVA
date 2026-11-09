import subprocess
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'app/python'))
from desktop_reading import DesktopDBusActions, DesktopReadingDispatcher, HeldFistDetector
from queued_reading import QueuedReadingDispatcher
from test_reading_swipe import hand
from reading_swipe import palm_sample_info
from test_reading_navigation import pose


def fist(x=.4775):
    return hand(x, closed=True)


def perform_fist(detector, start=0.):
    values = [(pose(.5), start+t) for t in (0., .25, .5)]
    values += [(fist(), start+t) for t in (.75, 1., 1.25, 1.5, 1.75, 2.)]
    return [detector.update(p, t) for p, t in values]


class HeldFistTests(unittest.TestCase):
    def test_deliberate_hold_fires_once_and_never_toggles_each_frame(self):
        detector = HeldFistDetector()
        self.assertEqual(sum(perform_fist(detector)), 1)
        for index in range(50):
            self.assertFalse(detector.update(fist(), 2.25+index*.25))
        self.assertEqual(sum(perform_fist(detector, 16.)), 1)

    def test_closed_hand_at_start_and_short_fist_cannot_leave_presentation(self):
        detector = HeldFistDetector()
        for index in range(12):
            self.assertFalse(detector.update(fist(), index*.25))
        detector = HeldFistDetector()
        for p, t in [(pose(.5), 0.), (pose(.5), .25), (pose(.5), .5),
                     (fist(), .75), (fist(), 1.), (pose(.5), 1.25)]:
            self.assertFalse(detector.update(p, t))

    def test_hand_loss_stale_results_and_large_gaps_cancel_the_hold(self):
        for failure in ('loss', 'stale', 'gap'):
            detector = HeldFistDetector()
            for p, t in [(pose(.5), 0.), (pose(.5), .25), (pose(.5), .5), (fist(), .75)]:
                self.assertFalse(detector.update(p, t))
            if failure == 'loss':
                detector.update(None, 1.)
            elif failure == 'stale':
                detector.update(fist(), 1., age=1.01)
            else:
                detector.update(fist(), 1.8)
            for index in range(12):
                self.assertFalse(detector.update(fist(), 2.+index*.25))

    def test_moving_fist_off_center_hand_and_nan_cannot_trigger(self):
        for kind in ('moving', 'edge', 'nan'):
            detector = HeldFistDetector()
            perform_fist_start = [(pose(.5), 0.), (pose(.5), .25), (pose(.5), .5)]
            for p, t in perform_fist_start:
                detector.update(p, t)
            for index in range(12):
                p = fist(.4775+(.02*index if kind == 'moving' else .35 if kind == 'edge' else 0.))
                if kind == 'nan':
                    p[9].x = float('nan')
                self.assertFalse(detector.update(p, .75+index*.25))

    def test_pointing_finger_is_not_a_closed_fist(self):
        detector = HeldFistDetector()
        for index in range(3):
            detector.update(pose(.5), index*.25)
        for index in range(12):
            p = fist()
            p[5:9] = pose(.5)[5:9]
            self.assertFalse(detector.update(p, .75+index*.25))


    def test_closing_pose_shift_starts_fresh_hold_without_relaxing_movement_limit(self):
        detector = HeldFistDetector()
        for t in (0., .3, .6):
            self.assertFalse(detector.update(pose(.5),t,age=.3))
        events=[]
        for t in (.9,1.2,1.5,1.8,2.1):
            closed=fist()
            # Relative palm displacement while the wrist stays put: changing
            # hand pose is distinct from moving the entire hand through space.
            for point in closed[1:]:point.x-=.07
            events.append(detector.update(closed,t,age=.3))
        self.assertEqual(sum(events),1)
        values=detector.diagnostics()
        self.assertAlmostEqual(values['desktop_palm_distance'],0.)
        self.assertAlmostEqual(values['desktop_wrist_distance'],0.)
        self.assertAlmostEqual(values['desktop_anchor_distance'],0.)
        self.assertEqual(values['desktop_anchor_role'],'holding')
        self.assertEqual(detector.MAX_MOVEMENT,.045)

    def test_true_translation_cancels_and_keeps_prior_hold_diagnostics(self):
        detector=HeldFistDetector()
        for t in (0.,.25,.5):detector.update(pose(.5),t)
        for t in (.75,1.):detector.update(fist(),t)
        moved=fist()
        for point in moved:point.x-=.06
        self.assertFalse(detector.update(moved,1.25))
        cancelled=detector.diagnostics()
        self.assertEqual(cancelled['desktop_reset_reason'],'anchor_movement')
        self.assertEqual(cancelled['desktop_reset_count'],1)
        for key in ('anchor_distance','palm_distance','wrist_distance'):
            self.assertAlmostEqual(cancelled['desktop_last_reset_'+key],.06)
        self.assertAlmostEqual(cancelled['desktop_last_reset_hold_seconds'],.5)
        detector.update(None,1.5)
        detector.update(fist(),1.75)
        self.assertEqual(detector.diagnostics()['desktop_reset_reason'],'anchor_movement')
        self.assertEqual(detector.diagnostics()['desktop_last_reset_hold_seconds'],.5)
        self.assertTrue(all(value is None or isinstance(value,(str,int,float,bool))
                            for value in detector.diagnostics().values()))

    def test_invalid_wrist_cannot_arm_or_complete_a_fist(self):
        for bad in (-.1,float('nan'),float('inf')):
            detector=HeldFistDetector()
            for t in (0.,.25,.5):detector.update(pose(.5),t)
            for index in range(8):
                closed=fist();closed[0].x=bad
                self.assertFalse(detector.update(closed,.75+index*.25))
            self.assertEqual(detector.phase,'await_open')
            values=detector.diagnostics()
            self.assertTrue(values['desktop_last_reset_used_previous_geometry'])
            self.assertGreaterEqual(values['desktop_last_reset_openness'],155)

    def test_folded_joint_margin_does_not_require_open_swipe_visibility(self):
        detector=HeldFistDetector()
        for t in (0.,.25,.5):detector.update(hand(.4775,y=.23),t)
        events=[]
        for t in (.75,1.,1.25,1.5,1.75,2.):
            closed=hand(.4775,y=.23,closed=True)
            for m in (5,9,13,17):closed[m+2].y=-.005
            self.assertIsNone(palm_sample_info(closed)[0])
            events.append(detector.update(closed,t))
        self.assertEqual(sum(events),1)
        # Neither arbitrary off-screen nor excessively distant finite joints
        # inherit that small bounded prediction allowance.
        for x,y in ((.5,-.5),(.98,.17)):
            bad=hand(.4775,y=.23,closed=True)
            bad[7].x,bad[7].y=x,y
            self.assertIsNone(HeldFistDetector.sample(bad,.75)[0])

    def test_initial_open_hand_still_requires_visible_finger_joints(self):
        detector=HeldFistDetector()
        for index in range(6):
            incomplete=hand(.4775,y=.23)
            for m in (5,9,13,17):incomplete[m+3].y=-.005
            self.assertFalse(detector.update(incomplete,index*.25))
        for index in range(8):
            self.assertFalse(detector.update(hand(.4775,y=.23,closed=True),1.5+index*.25))



    def test_observed_open_movement_and_brief_bad_transition_can_reach_hold(self):
        detector=HeldFistDetector()
        def observation(shift,openness,closed):
            points=fist() if closed else pose(.5)
            for point in points:point.x-=shift
            sample,error=HeldFistDetector.sample(points,.75)
            self.assertIsNone(error)
            sample.update(openness=openness,fingers_closed=closed)
            return sample,None
        # Observed scalar values/timing, with intermediate tracking samples;
        # these are not claimed to be a raw-landmark recording of the user.
        frames=[(0.,observation(0.,176.,False)),
                (.5666,observation(.0648,175.944,False)),
                (.8666,observation(.082,175.944,False)),
                (1.1666,observation(.1147,175.944,False)),
                (1.4666,observation(.12,155.,False))]
        for timestamp,sample in frames:
            with patch.object(detector,'sample',return_value=sample):
                self.assertFalse(detector.update(None,timestamp,age=.2828))
        self.assertTrue(detector.armed)
        self.assertEqual(detector.reset_count,0)
        for timestamp in (1.7666,2.0666,2.3666,2.7156):
            with patch.object(detector,'sample',return_value=observation(.12,117.5358,True)):
                self.assertFalse(detector.update(None,timestamp,age=.28))
        with patch.object(detector,'sample',return_value=(None,'palm_size')):
            self.assertFalse(detector.update(None,2.9306,age=.28))
        self.assertTrue(detector.armed)
        self.assertEqual(detector.phase,'closing')
        events=[]
        for timestamp in (3.1446,3.4446,3.7446,4.0446,4.3446):
            with patch.object(detector,'sample',return_value=observation(.12,111.6099,True)):
                events.append(detector.update(None,timestamp,age=.28))
        self.assertEqual(sum(events),1)
        self.assertEqual(detector.reset_count,0)
        self.assertEqual(detector.diagnostics()['desktop_transition_invalid_count'],1)

    def test_palm_body_size_survives_narrow_knuckle_x_span_but_rejects_tiny_hand(self):
        closed=fist()
        for point in closed:point.x=.4775+(point.x-.4775)*.15
        sample,error=HeldFistDetector.sample(closed,.75)
        self.assertIsNone(error)
        self.assertLess(sample['palm_x_span'],.04)
        self.assertGreater(sample['palm_size'],.04)
        self.assertTrue(sample['fingers_closed'])
        detector=HeldFistDetector()
        for t in (0.,.25,.5):detector.update(pose(.5),t)
        self.assertEqual(sum(detector.update(closed,t) for t in (.75,1.,1.25,1.5,1.75,2.)),1)
        tiny=fist()
        for point in tiny:
            point.x=.4775+(point.x-.4775)*.1
            point.y=.6+(point.y-.6)*.1
        self.assertEqual(HeldFistDetector.sample(tiny,.75)[1],'palm_size')

    def test_closing_deadline_and_per_finger_diagnostics_are_bounded(self):
        detector=HeldFistDetector()
        for t in (0.,.25,.5):detector.update(pose(.5),t)
        for t in (.75,1.,1.25,1.5,1.75,2.,2.25,2.5):
            sample,error=HeldFistDetector.sample(fist(),.75)
            sample['openness']=117.5358
            with patch.object(detector,'sample',return_value=(sample,error)):
                self.assertFalse(detector.update(None,t))
        self.assertFalse(detector.armed)
        self.assertEqual(detector.diagnostics()['desktop_reset_reason'],'transition_timeout')
        for name in ('index','middle','ring','pinky'):
            self.assertIsInstance(detector.diagnostics()['desktop_'+name+'_angle'],float)
        for t in (2.75,3.,3.25,3.5,3.75,4.):
            self.assertFalse(detector.update(fist(),t))



    def test_three_visible_fingers_prepare_but_two_or_unbounded_fourth_do_not(self):
        border=hand(.4775,y=.23);border[8].y=.005
        sample,error=HeldFistDetector.sample(border,.75)
        self.assertIsNone(error);self.assertTrue(sample['open_visible'])
        self.assertEqual(sample['open_visible_fingers'],3)
        detector=HeldFistDetector()
        for t in (0.,.25,.5):self.assertFalse(detector.update(border,t))
        self.assertTrue(detector.armed)
        border[12].y=.005
        sample,error=HeldFistDetector.sample(border,.75)
        self.assertIsNone(error);self.assertFalse(sample['open_visible'])
        self.assertEqual(sample['open_visible_fingers'],2)
        detector=HeldFistDetector()
        for t in (0.,.25,.5):self.assertFalse(detector.update(border,t))
        self.assertFalse(detector.armed)
        self.assertIn('2/3',detector.preparation_feedback())
        for x,y in ((1.04,.2),(.99,.2),(float('nan'),.2)):
            bad=hand(.4775,y=.23);bad[8].x,bad[8].y=x,y
            self.assertIsNone(HeldFistDetector.sample(bad,.75)[0])

    def test_open_startup_budget_requires_both_fresh_central_open_endpoints(self):
        for gap,expected in ((1.2,True),(1.2001,False)):
            detector=HeldFistDetector();detector.update(pose(.5),0.,age=.3)
            self.assertFalse(detector.update(pose(.5),gap,age=.3))
            self.assertEqual(detector.armed,expected)
            self.assertEqual(detector.diagnostics()['desktop_open_gap_preserved'],expected)
        for bad in (1.0001,-.01,float('nan')):
            for endpoint in ('first','second'):
                detector=HeldFistDetector()
                detector.update(pose(.5),0.,age=bad if endpoint=='first' else .3)
                detector.update(pose(.5),.9535,age=bad if endpoint=='second' else .3)
                self.assertFalse(detector.armed,(endpoint,bad))
                self.assertFalse(detector.diagnostics()['desktop_open_gap_preserved'])
        for first,second in ((pose(.8),pose(.5)),(pose(.5),pose(.8)),
                             (fist(),pose(.5)),(pose(.5),fist())):
            detector=HeldFistDetector();detector.update(first,0.,age=.3)
            detector.update(second,.9535,age=.3)
            self.assertFalse(detector.armed)
            self.assertFalse(detector.diagnostics()['desktop_open_gap_preserved'])

    def test_armed_closing_and_holding_keep_original_gap_limit(self):
        for phase in ('armed','closing','holding'):
            detector=HeldFistDetector()
            for t in (0.,.25,.5):detector.update(pose(.5),t,age=.3)
            previous=.5
            if phase!='armed':
                points=fist()
                if phase=='closing':points[5:9]=pose(.5)[5:9]
                detector.update(points,.75,age=.3);previous=.75
            self.assertEqual(detector.phase,phase)
            after=previous+.9535
            self.assertFalse(detector.update(pose(.5),after,age=.3))
            self.assertEqual(detector.last_reset_reason,'frame_gap')
            self.assertFalse(detector.armed)
            self.assertFalse(detector.diagnostics()['desktop_open_gap_preserved'])
            for index in range(8):
                self.assertFalse(detector.update(fist(),after+.25+index*.25,age=.3))

    def test_bad_intermediate_frames_cannot_borrow_startup_preparation(self):
        for issue in ('loss','geometry','repeat','backwards'):
            detector=HeldFistDetector();detector.update(pose(.5),0.,age=.3)
            if issue=='loss':
                detector.update(None,.2,age=.3);after=1.1535
            elif issue=='geometry':
                bad=pose(.5);bad[7].x=2.
                detector.update(bad,.2,age=.3);after=1.1535
            else:
                detector.update(pose(.5),0. if issue=='repeat' else -.1,age=.3);after=.9535
            detector.update(pose(.5),after,age=.3)
            self.assertFalse(detector.armed,issue)
            self.assertFalse(detector.diagnostics()['desktop_open_gap_preserved'],issue)



    def observed_fist(self,detector,timestamp,openness,angles,shift=0.,age=.28):
        # Real reported angle scalars with stationary synthetic geometry;
        # no raw-camera landmark recording is represented by this fixture.
        points=fist()
        for point in points:point.x-=shift
        sample,error=HeldFistDetector.sample(points,.75)
        self.assertIsNone(error)
        sample.update(openness=openness,fingers_closed=all(value<=135 for value in angles),
                      index_angle=angles[0],middle_angle=angles[1],ring_angle=angles[2],
                      pinky_angle=angles[3],max_finger_angle=max(angles))
        with patch.object(detector,'sample',return_value=(sample,None)):
            return detector.update(points,timestamp,age=age)

    def test_real_near_threshold_samples_continue_hold_without_strict_evidence(self):
        detector=HeldFistDetector()
        for t in (0.,.25,.5):detector.update(pose(.5),t)
        strict=(99.37,(100.,102.,98.,99.))
        near=[(115.9833,(120.,133.65,110.,111.)),
              (113.10,(139.8709,108.,102.,103.)),
              (118.7079,(118.,120.,128.61,115.))]
        events=[]
        for index in range(3):
            events.append(self.observed_fist(detector,.75+index*.5,*strict))
            events.append(self.observed_fist(detector,1.+index*.5,*near[index]))
            self.assertEqual(detector.samples,index+1)
            self.assertEqual(detector.soft_samples,index+1)
            self.assertEqual(detector.closed_since,.75)
        self.assertFalse(any(events))
        values=detector.diagnostics()
        self.assertEqual(values['desktop_hold_samples'],3)
        self.assertEqual(values['desktop_hold_soft_samples'],3)
        self.assertGreaterEqual(values['desktop_hold_seconds'],1.2)
        self.assertEqual(values['desktop_decision_gate'],'holding_soft')
        self.assertIn('正在确认',detector.reason)
        self.assertNotIn('0.0秒',detector.reason)
        self.assertTrue(self.observed_fist(detector,2.25,*strict))
        self.assertEqual(detector.samples,4)
        self.assertEqual(detector.reset_count,0)
        # Latched still has closed_since: neither strict nor soft may trigger again.
        for index in range(20):
            self.assertFalse(self.observed_fist(detector,2.5+index*.25,*(strict if index%2 else near[0])))
        self.assertEqual(detector.phase,'latched')

        for t in (8.,8.25,8.5):self.assertFalse(detector.update(pose(.5),t))
        self.assertEqual(sum(self.observed_fist(detector,t,*strict) for t in (8.75,9.,9.25,9.5,9.75,10.)),1)



    def test_soft_cannot_start_trigger_or_substitute_for_strict_samples(self):
        strict=(99.,(100.,102.,98.,99.))
        soft=(118.7079,(118.,120.,128.61,115.))
        detector=HeldFistDetector()
        for t in (0.,.25,.5):detector.update(pose(.5),t)
        for index in range(10):
            self.assertFalse(self.observed_fist(detector,.75+index*.25,*soft))
        self.assertIsNone(detector.closed_since)
        self.assertEqual(detector.samples,0)
        detector=HeldFistDetector()
        for t in (0.,.25,.5):detector.update(pose(.5),t)
        self.assertFalse(self.observed_fist(detector,.75,*strict))
        for index in range(8):
            self.assertFalse(self.observed_fist(detector,1.+index*.25,*soft))
        self.assertEqual(detector.samples,1)
        for timestamp,count in ((3.,2),(3.25,3)):
            self.assertFalse(self.observed_fist(detector,timestamp,*strict))
            self.assertEqual(detector.samples,count)
            self.assertIn('正在确认',detector.reason)
            self.assertNotIn('0.0秒',detector.reason)
        self.assertTrue(self.observed_fist(detector,3.5,*strict))
        detector=HeldFistDetector()
        for t in (0.,.25,.5):detector.update(pose(.5),t)
        for t in (.75,1.,1.25,1.5):self.assertFalse(self.observed_fist(detector,t,*strict))
        self.assertEqual(detector.samples,4)
        for t in (1.75,2.):self.assertFalse(self.observed_fist(detector,t,*soft))
        self.assertGreaterEqual(detector.metrics['hold_seconds'],1.2)
        self.assertTrue(self.observed_fist(detector,2.25,*strict))

    def test_strict_and_release_boundaries_and_real_hard_open_clear_hold(self):
        detector=HeldFistDetector()
        for t in (0.,.25,.5):detector.update(pose(.5),t)
        self.assertFalse(self.observed_fist(detector,.75,115.,(135.,)*4))
        self.assertEqual(detector.samples,1)
        self.assertFalse(self.observed_fist(detector,1.,125.,(145.,)*4))
        self.assertEqual(detector.samples,1)
        self.assertEqual(detector.soft_samples,1)
        for openness,angles in ((125.0001,(140.,)*4),(100.,(145.0001,100.,100.,100.)),
                                (138.435,(100.,100.,155.77,160.474)),(110.,(180.,90.,90.,90.))):
            detector=HeldFistDetector()
            for t in (0.,.25,.5):detector.update(pose(.5),t)
            self.observed_fist(detector,.75,99.,(100.,)*4)
            self.assertFalse(self.observed_fist(detector,1.,openness,angles))
            self.assertIsNone(detector.closed_since)
            self.assertEqual((detector.samples,detector.soft_samples),(0,0))
            self.assertEqual(detector.last_reset_reason,'fist_incomplete')
            self.assertFalse(self.observed_fist(detector,1.25,99.,(100.,)*4))
            self.assertEqual(detector.samples,1)
            self.assertEqual(detector.metrics['hold_seconds'],0.)

    def test_soft_continuation_does_not_bypass_tracking_or_motion_resets(self):
        for failure in ('stale','loss','gap','geometry','offcenter','movement'):
            detector=HeldFistDetector()
            for t in (0.,.25,.5):detector.update(pose(.5),t)
            self.observed_fist(detector,.75,99.,(100.,)*4)
            self.observed_fist(detector,1.,118.,(130.,)*4)
            after=1.25
            if failure=='stale':
                self.observed_fist(detector,after,118.,(130.,)*4,age=1.0001)
            elif failure=='loss':detector.update(None,after,age=.28)
            elif failure=='gap':
                after=1.900001
                self.observed_fist(detector,after,118.,(130.,)*4)
            elif failure=='geometry':
                bad=fist();bad[7].x=2.
                detector.update(bad,after,age=.28)
            else:self.observed_fist(detector,after,118.,(130.,)*4,shift=.3 if failure=='offcenter' else .119)
            self.assertIsNone(detector.closed_since,failure)
            self.assertEqual((detector.samples,detector.soft_samples),(0,0),failure)
            if failure=='movement':
                self.assertEqual(detector.last_reset_reason,'anchor_movement')
                self.assertAlmostEqual(detector.diagnostics()['desktop_last_reset_anchor_distance'],.119)
            for index in range(6):
                self.assertFalse(self.observed_fist(detector,after+.25+index*.25,99.,(100.,)*4),failure)



class DesktopActionTests(unittest.TestCase):
    def test_idempotent_minimize_is_confirmed_without_a_global_toggle(self):
        results = [subprocess.CompletedProcess([], 0, '', ''),
                   subprocess.CompletedProcess([], 1, '', '')]
        with patch('desktop_reading.subprocess.run', side_effect=results) as run:
            action = DesktopDBusActions()
            self.assertTrue(action.show_desktop())
            action.close()
        self.assertEqual([call.args[0] for call in run.call_args_list], [
            ['/usr/bin/wlrctl', 'toplevel', 'minimize', 'state:unminimized'],
            ['/usr/bin/wlrctl', 'toplevel', 'find', 'state:unminimized']])

    def test_already_minimized_no_match_requires_state_confirmation(self):
        results = [subprocess.CompletedProcess([], 1, '', ''),
                   subprocess.CompletedProcess([], 1, '', '')]
        with patch('desktop_reading.subprocess.run', side_effect=results) as run:
            self.assertTrue(DesktopDBusActions().show_desktop())
        self.assertEqual(run.call_count, 2)
        self.assertEqual(run.call_args.args[0][2], 'find')

    def test_compositor_error_is_not_misreported_as_no_visible_windows(self):
        for results in ([subprocess.CompletedProcess([], 1, '', 'protocol missing')],
                        [subprocess.CompletedProcess([], 0, '', ''),
                         subprocess.CompletedProcess([], 1, '', 'connection lost')]):
            with self.subTest(results=results), patch('desktop_reading.subprocess.run', side_effect=results) as run:
                with self.assertRaises(RuntimeError):
                    DesktopDBusActions().show_desktop()
                self.assertEqual(sum(call.args[0][2] == 'minimize' for call in run.call_args_list), 1)

    def test_unconfirmed_minimize_is_not_replayed(self):
        def run(args, **kwargs):
            return subprocess.CompletedProcess(args, 0, '', '')
        ticks = iter([0., .5, 1., 1.5, 2., 2.5])
        with patch('desktop_reading.subprocess.run', side_effect=run) as request, \
                patch('desktop_reading.time.monotonic', side_effect=lambda: next(ticks)), \
                patch('desktop_reading.time.sleep'):
            with self.assertRaisesRegex(RuntimeError, 'did not confirm'):
                DesktopDBusActions().show_desktop()
        self.assertEqual(sum(call.args[0][2] == 'minimize' for call in request.call_args_list), 1)


class Reader:
    def __init__(self):
        self.owner = ':1.1'
        self.session = 'presentation-1'
        self.active = True
        self.now = 100.
        self.visibility = {}
        self.page = 13
        self.calls = []

    def state(self):
        return dict(active=self.active, page=self.page, total=32,
                    reader_owner=self.owner, session_id=self.session, **self.visibility)

    def goto(self, page, owner):
        self.page = page
        self.calls.append((page, owner))
        return self.state()


class DesktopIntegrationTests(unittest.TestCase):
    def make(self, fail=False):
        reader, events, desktops = Reader(), [], []
        def emit(event, **data):
            events.append(dict(event=event, **data))
        def show():
            desktops.append(True)
            if fail:
                raise RuntimeError('desktop reply was lost')
            return True
        navigation = QueuedReadingDispatcher(reader, emit)
        dispatcher = DesktopReadingDispatcher(navigation, reader, show, emit, clock=lambda:reader.now)
        self.addCleanup(dispatcher.close)
        return reader, dispatcher, events, desktops

    def perform(self, dispatcher, start=0.):
        for p, t in [(pose(.5), start+t) for t in (0., .25, .5)]+[
                (fist(), start+t) for t in (.75, 1., 1.25, 1.5, 1.75, 2.)]:
            dispatcher.process(p, t)

    def idle(self, dispatcher):
        navigation = dispatcher.navigation
        with navigation.condition:
            self.assertTrue(navigation.condition.wait_for(
                lambda: not navigation.inflight and navigation.target is None, timeout=2))

    def test_pause_does_not_flip_hidden_slides_or_toggle_again_then_reopen_resumes(self):
        reader, dispatcher, events, desktops = self.make()
        self.perform(dispatcher)
        self.assertEqual(len(desktops), 1)
        self.assertIsNotNone(dispatcher.paused_identity)
        for index, x in enumerate([.5, .5, .5, .58, .66, .72, .6, .5]):
            dispatcher.process(pose(x), 3.+index*.25)
        self.assertEqual(reader.calls, [])
        self.perform(dispatcher, 6.)
        self.assertEqual(len(desktops), 1)
        # OpenPresentation can reuse the bus owner; the new session_id matters.
        reader.session = 'presentation-2'
        reader.now=101.
        reader.visibility=dict(presentation_visible=True,visibility_source='wayland_exact_window',visibility_checked_at=100.5)
        for index, x in enumerate([.5, .5, .5, .58, .66, .72, .6, .5]):
            dispatcher.process(pose(x), 10.+index*.25)
        self.idle(dispatcher)
        self.assertEqual(reader.calls, [(14, ':1.1')])
        self.assertTrue(any(e['event'] == 'reading_resumed' for e in events))

    def test_timeout_is_not_retried_while_user_keeps_fist_closed(self):
        reader, dispatcher, events, desktops = self.make(fail=True)
        self.perform(dispatcher)
        for index in range(20):
            dispatcher.process(fist(), 2.25+index*.25)
        for index, x in enumerate([.5, .5, .5, .58, .66, .72, .6, .5]):
            dispatcher.process(pose(x), 8.+index*.25)
        self.assertEqual(len(desktops), 1)
        self.assertEqual(reader.calls, [])
        self.assertIsNotNone(dispatcher.paused_identity)
        self.assertTrue(any(e['event'] == 'action_error' and not e['automatic_retry'] for e in events))

    def test_recorded_startup_gap_and_one_border_finger_prepare_then_hold_once(self):
        reader,dispatcher,events,desktops=self.make()
        opened=hand(.4775,y=.23)
        dispatcher.process(opened,0.,age=.9499)
        self.assertIn('握拳准备',dispatcher.controller.reason)
        border=hand(.4775,y=.23)
        border[8].y=.005  # In-frame/bounded, outside the 1% full-visibility inset.
        dispatcher.process(border,.9535,age=.2692)
        values=dispatcher.diagnostics()
        self.assertTrue(values['desktop_open_gap_preserved'])
        self.assertTrue(values['desktop_prepared'])
        self.assertTrue(values['desktop_open_visible'])
        self.assertFalse(values['desktop_open_fully_visible'])
        self.assertEqual(values['desktop_open_visible_fingers'],3)
        self.assertEqual(values['desktop_open_border_mask'],1)
        self.assertAlmostEqual(values['desktop_open_frame_margin'],.005)
        self.assertIn('握拳返回桌面已就绪',dispatcher.controller.reason)
        self.assertEqual(desktops,[])
        for timestamp in (1.2,1.5,1.8,2.1):
            dispatcher.process(hand(.4775,y=.23,closed=True),timestamp,age=.28)
            self.assertEqual(desktops,[])
        dispatcher.process(hand(.4775,y=.23,closed=True),2.4,age=.28)
        self.idle(dispatcher)
        self.assertEqual(len(desktops),1)
        self.assertEqual(reader.calls,[])
        self.assertEqual(dispatcher.detector.reset_count,0)
        other,not_ready,unused,actions=self.make()
        two=hand(.4775,y=.23);two[8].y=two[12].y=.005
        for timestamp in (0.,.25,.5):not_ready.process(two,timestamp,age=.3)
        self.assertIn('握拳未就绪',not_ready.controller.reason)
        self.assertIn('2/3',not_ready.controller.reason)
        self.assertFalse(not_ready.detector.armed)
        self.assertEqual(actions,[])

    def test_manual_restore_same_session_resumes_only_after_exact_visible_probe(self):
        reader,dispatcher,events,desktops=self.make()
        self.perform(dispatcher)
        self.assertEqual(dispatcher.pause_completed_at,100.)
        reader.now=101.
        reader.visibility=dict(presentation_visible=False,visibility_source='wayland_exact_window',visibility_checked_at=100.5)
        dispatcher.process(fist(),3.)
        self.assertIsNotNone(dispatcher.paused_identity)
        self.assertEqual(reader.calls,[])
        reader.visibility['presentation_visible']=True
        dispatcher.process(fist(),3.6)
        self.assertIsNone(dispatcher.paused_identity)
        self.assertEqual(len(desktops),1)
        self.assertEqual(reader.calls,[])
        self.assertTrue(any(e.get('delivery')=='visible_managed_presentation' for e in events))
        for index,x in enumerate([.5,.5,.5,.58,.66,.72,.6,.5]):
            dispatcher.process(pose(x),5.+index*.25)
        self.idle(dispatcher)
        self.assertEqual(reader.calls,[(14,':1.1')])
        self.assertEqual(len(desktops),1)

    def test_probe_started_during_minimize_cannot_unpause(self):
        reader,dispatcher,events,desktops=self.make()
        show=dispatcher.show_desktop
        def delayed():
            reader.visibility=dict(presentation_visible=True,visibility_source='wayland_exact_window',visibility_checked_at=102.)
            reader.now=103.
            return show()
        dispatcher.show_desktop=delayed
        self.perform(dispatcher)
        self.assertEqual(dispatcher.pause_completed_at,103.)
        reader.now=103.2
        dispatcher.process(fist(),3.)
        self.assertIsNotNone(dispatcher.paused_identity)
        self.assertEqual(dispatcher.resume_gate,'visibility_before_desktop_completed')
        reader.visibility['visibility_checked_at']=103.1
        dispatcher.process(fist(),3.6)
        self.assertIsNone(dispatcher.paused_identity)
        self.assertEqual(len(desktops),1)

    def test_hidden_unknown_stale_future_or_other_window_cannot_resume_even_new_session(self):
        reader,dispatcher,events,desktops=self.make()
        self.perform(dispatcher)
        reader.session='different-session'
        valid=dict(presentation_visible=True,visibility_source='wayland_exact_window',visibility_checked_at=100.5)
        variants=[dict(presentation_visible=False),dict(presentation_visible=1),
                  dict(visibility_source='any_active_window'),dict(visibility_source='unavailable'),
                  dict(visibility_checked_at=None),dict(visibility_checked_at=True),
                  dict(visibility_checked_at=float('nan')),dict(visibility_checked_at=float('inf')),
                  dict(visibility_checked_at='100.5'),dict(visibility_checked_at=99.),
                  dict(visibility_checked_at=100.),dict(visibility_checked_at=102.)]
        for index,change in enumerate(variants):
            reader.now=101.
            reader.visibility=dict(valid,**change)
            dispatcher.process(pose(.5),3.+index*.6)
            self.assertIsNotNone(dispatcher.paused_identity,change)
        reader.visibility=valid
        reader.now=103.
        dispatcher.process(pose(.5),12.)
        self.assertIsNotNone(dispatcher.paused_identity)
        reader.now=101.;reader.active=False
        dispatcher.process(pose(.5),12.6)
        self.assertIsNotNone(dispatcher.paused_identity)
        reader.active=True;reader.visibility={}
        dispatcher.process(pose(.5),13.2)
        self.assertIsNotNone(dispatcher.paused_identity)
        self.assertEqual(reader.calls,[])
        self.assertEqual(len(desktops),1)

    def test_failed_desktop_delivery_also_uses_completion_time_and_does_not_retry(self):
        reader,dispatcher,events,desktops=self.make(fail=True)
        failed=dispatcher.show_desktop
        def delayed_failure():
            reader.now=105.
            return failed()
        dispatcher.show_desktop=delayed_failure
        self.perform(dispatcher)
        self.assertEqual(dispatcher.pause_completed_at,105.)
        reader.now=105.2
        reader.visibility=dict(presentation_visible=True,visibility_source='wayland_exact_window',visibility_checked_at=104.5)
        dispatcher.process(fist(),3.)
        self.assertIsNotNone(dispatcher.paused_identity)
        reader.visibility['visibility_checked_at']=105.1
        dispatcher.process(fist(),3.6)
        self.assertIsNone(dispatcher.paused_identity)
        self.assertEqual(len(desktops),1)
        self.assertEqual(reader.calls,[])

    def test_normal_left_right_swipes_still_work(self):
        reader, dispatcher, _, desktops = self.make()
        for index, x in enumerate([.5,.5,.5,.42,.34,.28,.4,.5,.5,.58,.66,.72,.6,.5]):
            dispatcher.process(pose(x), index*.25)
            self.idle(dispatcher)
        self.assertEqual([page for page, _ in reader.calls], [12, 13])
        self.assertEqual(desktops, [])

    def test_missing_reader_never_toggles_other_windows(self):
        reader, dispatcher, events, desktops = self.make()
        reader.active = False
        self.perform(dispatcher)
        self.assertEqual(desktops, [])
        self.assertTrue(any(e['event'] == 'action_error' for e in events))


if __name__ == '__main__':
    unittest.main()
