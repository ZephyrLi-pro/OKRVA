import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "app" / "python"))

import unittest
from latest_frame import LatestFrame
from reading_dispatch import ReadingDispatcher
from test_reading_navigation import pose


class ReadingDispatchTests(unittest.TestCase):
    def test_actual_actions_only_on_outward_strokes(self):
        actions, events = [], []
        dispatcher = ReadingDispatcher(actions.append,
            lambda event, **data: events.append((event, data)))
        for index, x in enumerate([.5,.5,.5,.42,.34,.28,.4,.5,.5,.58,.66,.72,.6,.5]):
            dispatcher.process(pose(x), index*.25)
        self.assertEqual(actions, ['left', 'right'])
        self.assertEqual([event for event, data in events],
                         ['gesture_recognized','action_candidate','gesture_recognized','action_candidate'])

    def test_failed_action_is_reported_and_never_automatically_replayed(self):
        calls, events = [], []
        def fail(direction):
            calls.append(direction)
            raise RuntimeError('native receiver unavailable')
        dispatcher = ReadingDispatcher(fail,
            lambda event, **data: events.append((event, data)))
        for index, x in enumerate([.5,.5,.5,.42,.34,.28,.28,.28,.4,.5]):
            self.assertIsNone(dispatcher.process(pose(x), index*.25))
        self.assertEqual(calls, ['left'])
        errors=[data for event,data in events if event=='action_error']
        self.assertEqual(len(errors),1)
        self.assertFalse(errors[0]['automatic_retry'])

    def test_stale_camera_result_cannot_execute_or_join_a_trace(self):
        actions = []
        dispatcher = ReadingDispatcher(actions.append, lambda *a, **kw: None)
        dispatcher.process(pose(.5), 0.)
        dispatcher.process(pose(.5), .25)
        dispatcher.process(pose(.47), .5)
        dispatcher.process(pose(.34), .75, age=1.01)
        dispatcher.process(pose(.28), 1.)
        self.assertEqual(actions, [])

    def test_camera_mailbox_drops_old_frames_instead_of_queuing(self):
        mailbox = LatestFrame()
        for index in range(100):
            mailbox.publish(index, index*.01)
        self.assertEqual(mailbox.wait_after(0), (100,.99,99))
        self.assertIsNone(mailbox.wait_after(100, timeout=0))
        mailbox.publish('new', 1.)
        self.assertEqual(mailbox.wait_after(100), (101,1.,'new'))

    def test_camera_failure_propagates_and_closed_mailbox_drops_frames(self):
        mailbox = LatestFrame()
        mailbox.close('camera disconnected')
        mailbox.publish('late', 1.)
        self.assertEqual(mailbox.sequence, 0)
        with self.assertRaisesRegex(RuntimeError, 'camera disconnected'):
            mailbox.wait_after(0, timeout=0)


if __name__ == '__main__':
    unittest.main()
