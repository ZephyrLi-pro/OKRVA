import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "app" / "python"))

from dataclasses import dataclass
import math
import unittest
from reading_swipe import ReadingSwipeDetector, palm_sample


@dataclass
class Point:
    x: float
    y: float
    z: float=0


def hand(x=.5,y=.6,closed=False):
    p=[Point(x,y) for _ in range(21)]
    for index,dx in [(1,-.09),(5,-.05),(9,0),(13,.05),(17,.09)]:
        for k in range(4):
            p[index+k]=Point(x+dx,y-.06-k*.045)
    if closed:
        for m in [5,9,13,17]:
            p[m+2]=Point(p[m].x+.025,p[m].y-.025)
            p[m+3]=Point(p[m].x,p[m].y)
    return p


class ReadingSwipeTests(unittest.TestCase):
    def test_upright_open_palm_accepts_both_directions(self):
        for raw_xs,expected in [([.42,.48,.56],'left'),([.58,.52,.44],'right')]:
            d=ReadingSwipeDetector()
            result=[d.update(hand(x),i*.2) for i,x in enumerate(raw_xs)]
            self.assertEqual(result,[None,None,expected])

    def test_stationary_jitter_does_not_trigger(self):
        d=ReadingSwipeDetector()
        for i in range(50):
            self.assertIsNone(d.update(hand(.5+(.003 if i%2 else -.003)),i*.1))

    def test_vertical_and_diagonal_movement_do_not_trigger(self):
        for diagonal in [False,True]:
            d=ReadingSwipeDetector()
            for i in range(6):
                self.assertIsNone(d.update(hand(.5+i*.025 if diagonal else .5,.7-i*.04),i*.2))

    def test_closed_hand_is_rejected(self):
        d=ReadingSwipeDetector()
        for i in range(5):
            self.assertIsNone(d.update(hand(.4+i*.04,closed=True),i*.2))

    def test_short_two_sample_jump_is_not_a_stroke(self):
        d=ReadingSwipeDetector()
        self.assertIsNone(d.update(hand(.4),0))
        self.assertIsNone(d.update(hand(.56),.2))

    def test_tiny_motion_is_not_a_stroke(self):
        d=ReadingSwipeDetector()
        for i in range(5):
            self.assertIsNone(d.update(hand(.45+i*.015),i*.2))

    def test_missing_hand_cannot_join_two_positions(self):
        d=ReadingSwipeDetector()
        for p,t in [(hand(.4),0),(hand(.45),.2),(None,.3),(hand(.55),.4)]:
            self.assertIsNone(d.update(p,t))

    def test_long_camera_gap_cannot_join_positions(self):
        d=ReadingSwipeDetector()
        for x,t in [(.4,0),(.45,.2),(.56,1),(.6,1.2)]:
            self.assertIsNone(d.update(hand(x),t))

    def test_large_detection_jump_is_reset(self):
        d=ReadingSwipeDetector()
        for x,t in [(.2,0),(.25,.2),(.65,.4)]:
            self.assertIsNone(d.update(hand(x),t))

    def test_one_sustained_stroke_produces_one_event(self):
        d=ReadingSwipeDetector()
        events=[d.update(hand(.25+i*.025),i*.1) for i in range(20)]
        self.assertEqual([e for e in events if e],['left'])

    def test_second_stroke_after_settling(self):
        d=ReadingSwipeDetector()
        values=[(.4,0),(.46,.2),(.54,.4),(.54,1.3),(.54,1.6),(.54,1.9),(.48,2.1),(.4,2.3)]
        events=[d.update(hand(x),t) for x,t in values]
        self.assertEqual([e for e in events if e],['left','right'])

    def test_cooldown_survives_hand_loss(self):
        d=ReadingSwipeDetector()
        for x,t in [(.4,0),(.46,.1),(.54,.2)]:
            result=d.update(hand(x),t)
        self.assertEqual(result,'left')
        self.assertIsNone(d.update(None,.3))
        for x,t in [(.54,.4),(.48,.5),(.4,.6)]:
            self.assertIsNone(d.update(hand(x),t))

    def test_repeated_or_out_of_order_timestamps_are_ignored(self):
        d=ReadingSwipeDetector()
        self.assertIsNone(d.update(hand(.4),1))
        self.assertIsNone(d.update(hand(.5),1))
        self.assertIsNone(d.update(hand(.6),.5))
        self.assertEqual(len(d.history),1)

    def test_invalid_and_clipped_landmarks_rejected(self):
        for p in [hand(.03),hand(.97),hand()[:20]]:
            self.assertIsNone(palm_sample(p))
        p=hand();p[8].x=math.nan
        self.assertIsNone(palm_sample(p))

    def test_small_fingertip_edge_excursion_keeps_visible_palm(self):
        p=hand()
        p[4].x=-.015
        self.assertIsNotNone(palm_sample(p))

    def test_unobserved_thumb_does_not_discard_visible_palm_and_fingers(self):
        p=hand()
        p[4].x=-.2
        self.assertIsNotNone(palm_sample(p))

    def test_palm_knuckle_offscreen_is_rejected(self):
        p=hand()
        p[5].x=-.015
        self.assertIsNone(palm_sample(p))

    def test_forty_alternating_strokes_do_not_stop_after_fourteen(self):
        detector=ReadingSwipeDetector()
        events=[]
        now=0.
        for i in range(40):
            start,end=(.4,.56) if i%2==0 else (.56,.4)
            for offset,x in [(0,start),(.2,(start+end)/2),(.4,end),
                             (.6,end),(.8,end),(1.,end),(1.2,end),(1.4,end),(1.6,end)]:
                event=detector.update(hand(x),now+offset)
                if event:events.append(event)
            now+=1.8
        self.assertEqual(events,['left','right']*20)

    def test_offscreen_wrist_does_not_break_a_visible_open_palm_stroke(self):
        detector=ReadingSwipeDetector();events=[]
        for i,x in enumerate([.4,.46,.54]):
            p=hand(x);p[0].y=1.3
            event=detector.update(p,i*.2)
            if event:events.append(event)
        self.assertEqual(events,['left'])

    def test_wrist_predictions_cannot_move_the_motion_anchor(self):
        detector=ReadingSwipeDetector()
        for i,x in enumerate([-.3,.2,.7,1.2]):
            p=hand();p[0].x=x
            self.assertIsNone(detector.update(p,i*.2))

    def test_one_clipped_finger_does_not_break_other_three_visible_fingers(self):
        detector=ReadingSwipeDetector();events=[]
        for i,x in enumerate([.4,.46,.54]):
            p=hand(x);p[8].y=-.1
            event=detector.update(p,i*.2)
            if event:events.append(event)
        self.assertEqual(events,['left'])

    def test_two_visible_fingers_are_insufficient(self):
        p=hand();p[8].y=-.1;p[12].y=-.1
        self.assertIsNone(palm_sample(p))

    def test_rest_during_cooldown_rearms_the_next_stroke(self):
        detector=ReadingSwipeDetector()
        samples=[(.4,0),(.46,.2),(.54,.4),(.54,.6),(.54,.9),(.54,1.1),(.48,1.3),(.4,1.5)]
        events=[detector.update(hand(x),t) for x,t in samples]
        self.assertEqual([e for e in events if e],['left','right'])

    def test_continuous_alternating_strokes_without_stationary_holds(self):
        detector=ReadingSwipeDetector()
        xs=[.35,.43,.51,.59,.51,.43,.35,.43,.51,.59,.51,.43,.35]
        events=[detector.update(hand(x),i*.25) for i,x in enumerate(xs)]
        self.assertEqual([e for e in events if e],['left','right','left','right'])

    def test_small_reversal_then_same_stroke_is_not_a_second_event(self):
        detector=ReadingSwipeDetector()
        xs=[.3,.38,.46,.54,.52,.58,.66,.74]
        events=[detector.update(hand(x),i*.25) for i,x in enumerate(xs)]
        self.assertEqual([e for e in events if e],['left'])

    def test_slow_sustained_stroke_does_not_rearm_from_small_steps(self):
        detector=ReadingSwipeDetector()
        events=[detector.update(hand(.2+i*.015),i*.2) for i in range(35)]
        self.assertEqual([e for e in events if e],['left'])

    def test_priming_does_not_emit_or_latch(self):
        detector=ReadingSwipeDetector()
        detector.prime([(hand(.4),0),(hand(.48),.2),(hand(.56),.4)])
        self.assertEqual(detector.last_trigger,-math.inf)
        self.assertTrue(detector.armed)
        self.assertEqual(len(detector.history),3)


if __name__=='__main__':
    unittest.main()
