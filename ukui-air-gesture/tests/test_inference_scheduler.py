import sys
import unittest
from pathlib import Path

import numpy as np


PYTHON_DIR = Path(__file__).resolve().parents[1] / "app" / "python"
sys.path.insert(0, str(PYTHON_DIR))

from gesture_recognition import (PALM_SEARCH_INTERVAL_SEC, PALM_IDLE_SEARCH_INTERVAL_SEC,
                                 HandTracker)  # noqa: E402


class EmptyPalmDetector:
    def __init__(self):
        self.calls = 0

    def infer(self, _frame):
        self.calls += 1
        return []


class UnusedPoseDetector:
    def infer(self, _frame, _palm):
        raise AssertionError("pose detector should not run")


class InferenceSchedulerTest(unittest.TestCase):
    def quiet_tracker(self):
        palm = EmptyPalmDetector()
        tracker = HandTracker(palm, UnusedPoseDetector())
        frame = np.zeros((480, 640, 3), dtype=np.uint8)
        for now in (10., 10.5, 11., 11.5, 12.1):
            tracker.infer(frame, now=now)
        self.assertEqual(tracker.next_search_at, 12.1 + PALM_IDLE_SEARCH_INTERVAL_SEC)
        return tracker, palm, frame

    def test_quiet_empty_scene_waits_but_periodic_probe_still_runs(self):
        tracker, palm, frame = self.quiet_tracker()
        calls = palm.calls
        self.assertEqual(tracker.infer(frame, now=13.)[2], 'search-wait')
        self.assertEqual(tracker.infer(frame, now=15.)[2], 'search-wait')
        self.assertEqual(palm.calls, calls)
        tracker.infer(frame, now=15.1)
        self.assertEqual(palm.calls, calls + 1)

    def test_local_motion_starts_detection_before_quiet_deadline(self):
        tracker, palm, frame = self.quiet_tracker()
        changed = frame.copy()
        changed[150:300, 220:360] = 100
        self.assertEqual(tracker.infer(changed, now=12.2)[2], 'search-wait')
        calls = palm.calls
        self.assertEqual(tracker.infer(changed, now=12.6)[2], 'none')
        self.assertEqual(palm.calls, calls + 1)

    def test_exposure_and_isolated_noise_do_not_run_full_model(self):
        tracker, palm, frame = self.quiet_tracker()
        frame[:] = 40
        frame[10:12, 10:12] = 255
        calls = palm.calls
        self.assertEqual(tracker.infer(frame, now=13.)[2], 'search-wait')
        self.assertEqual(palm.calls, calls)

    def test_detected_hand_resumes_pose_tracking_without_quiet_wait(self):
        tracker, palm, frame = self.quiet_tracker()
        class Pose:
            def __init__(self): self.calls = 0
            def infer(self, *_):
                self.calls += 1
                hand = np.zeros(132)
                hand[4:67] = np.array([[200+i*3, 150+(i%5)*10, 0] for i in range(21)]).ravel()
                hand[-2:] = [.1, .9]
                return hand
        pose = Pose()
        tracker.pose_detector = pose
        palm.infer = lambda _: [np.zeros(19)]
        changed = frame.copy(); changed[150:300, 220:360] = 100
        self.assertIsNotNone(tracker.infer(changed, now=12.6)[0])
        self.assertIsNotNone(tracker.infer(changed, now=12.65)[0])
        self.assertEqual(pose.calls, 2)
        self.assertEqual(tracker.track_hits, 1)

    def test_no_hand_search_is_rate_limited(self):
        palm = EmptyPalmDetector()
        tracker = HandTracker(palm, UnusedPoseDetector())
        frame = np.zeros((480, 640, 3), dtype=np.uint8)

        self.assertEqual(tracker.infer(frame, now=10.0)[2], "none")
        self.assertEqual(palm.calls, 1)

        self.assertEqual(tracker.infer(frame, now=10.1)[2], "search-wait")
        self.assertEqual(tracker.infer(frame, now=10.39)[2], "search-wait")
        self.assertEqual(palm.calls, 1)

        self.assertEqual(
            tracker.infer(frame, now=10.0 + PALM_SEARCH_INTERVAL_SEC)[2],
            "none",
        )
        self.assertEqual(palm.calls, 2)


if __name__ == "__main__":
    unittest.main()
