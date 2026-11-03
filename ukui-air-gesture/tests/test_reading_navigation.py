import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "app" / "python"))

import unittest
from reading_navigation import ReadingNavigationController
from test_reading_swipe import hand


def pose(center):
    # Four MCP offsets [-.05,0,.05,.09] average raw_x + .0225.
    return hand(.9775-center)


def run(xs,period=.25):
    controller=ReadingNavigationController()
    events=[]
    for i,x in enumerate(xs):
        event=controller.update(None if x is None else pose(x),i*period)
        if event:events.append(event)
    return events,controller


class ReadingNavigationTests(unittest.TestCase):
    def test_left_then_return_to_center_does_not_count_right(self):
        events,_=run([.5,.42,.34,.28,.36,.45,.5])
        self.assertEqual(events,['left'])

    def test_right_then_return_to_center_does_not_count_left(self):
        events,_=run([.5,.58,.66,.72,.64,.55,.5])
        self.assertEqual(events,['right'])

    def test_repeated_left_excursions_without_stationary_hold(self):
        events,_=run([.5,.42,.34,.28,.4,.5,.42,.34,.28,.4,.5])
        self.assertEqual(events,['left','left'])

    def test_repeated_right_excursions_without_stationary_hold(self):
        events,_=run([.5,.58,.66,.72,.6,.5,.58,.66,.72,.6,.5])
        self.assertEqual(events,['right','right'])

    def test_alternating_excursions_count_only_the_outward_parts(self):
        events,_=run([.5,.42,.34,.28,.4,.5,.58,.66,.72,.6,.5])
        self.assertEqual(events,['left','right'])

    def test_initial_off_center_motion_does_not_trigger(self):
        events,_=run([.28,.2,.25,.32])
        self.assertEqual(events,[])

    def test_center_jitter_never_counts(self):
        events,_=run([.5+(.02 if i%2 else -.02) for i in range(80)])
        self.assertEqual(events,[])

    def test_holding_or_jittering_at_the_side_is_one_event(self):
        events,_=run([.5,.42,.34,.28]+[.28+(.01 if i%2 else -.01) for i in range(40)])
        self.assertEqual(events,['left'])

    def test_short_excursion_cancelled_by_return_to_center(self):
        # Eight percent is below the relative ten-percent stroke target.
        events,_=run([.5,.58,.58,.5,.5,.5])
        self.assertEqual(events,[])

    def test_pending_direction_completes_only_beyond_target(self):
        events,_=run([.5,.58,.58,.61])
        self.assertEqual(events,['right'])

    def test_hand_loss_requires_center_again(self):
        events,_=run([.5,.42,.34,.28,None,.28,.2,.3,.5,.58,.66,.72])
        self.assertEqual(events,['left','right'])

    def test_crossing_center_between_two_samples_can_rearm(self):
        events,_=run([.5,.42,.34,.28,.38,.62,.7,.72])
        self.assertEqual(events,['left','right'])

    def test_fast_stroke_after_stable_center_uses_neutral_observations(self):
        events,_=run([.5,.5,.5,.33])
        self.assertEqual(events,['left'])

    def test_neutral_preparation_motion_does_not_latch_the_core(self):
        events,_=run([.41,.49,.58,.67])
        self.assertEqual(events,['right'])


if __name__=='__main__':unittest.main()
