import sys
import unittest
from pathlib import Path

import numpy as np


PYTHON_DIR = Path(__file__).resolve().parents[1] / "app" / "python"
sys.path.insert(0, str(PYTHON_DIR))

from gesture_recognition import PALM_SEARCH_INTERVAL_SEC, HandTracker  # noqa: E402


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
