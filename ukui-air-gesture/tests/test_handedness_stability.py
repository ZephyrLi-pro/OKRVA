import sys
import unittest
from pathlib import Path


PYTHON_DIR = Path(__file__).resolve().parents[1] / "app" / "python"
sys.path.insert(0, str(PYTHON_DIR))

from gesture_recognition import (  # noqa: E402
    HANDEDNESS_EVIDENCE_WINDOW,
    HANDEDNESS_FORGET_FRAMES,
    BufferedGestureDispatcher,
    HandednessStabilizer,
)


class HandednessStabilizerTest(unittest.TestCase):
    def test_consistent_edge_clipped_samples_still_lock(self):
        stabilizer = HandednessStabilizer()

        for _ in range(3):
            self.assertEqual(stabilizer.update(0.1, 0.0), (None, None))
        self.assertEqual(stabilizer.update(0.1, 0.0), ("Left", "locked"))

    def test_single_low_quality_outlier_cannot_lock(self):
        stabilizer = HandednessStabilizer()

        self.assertEqual(stabilizer.update(0.9, 0.0), (None, None))
        self.assertEqual(stabilizer.update(0.1, 0.9), (None, None))
        self.assertEqual(stabilizer.update(0.1, 0.9), (None, None))
        self.assertEqual(stabilizer.update(0.1, 0.9), ("Left", "locked"))

    def test_weighted_evidence_locks_after_three_good_samples(self):
        stabilizer = HandednessStabilizer()

        self.assertEqual(stabilizer.update(0.1, 0.9), (None, None))
        self.assertEqual(stabilizer.update(0.1, 0.9), (None, None))
        self.assertEqual(stabilizer.update(0.1, 0.9), ("Left", "locked"))

    def test_conflicting_first_result_does_not_lock_wrong_hand(self):
        stabilizer = HandednessStabilizer()
        scores = [0.7237, 0.5333, 0.2765, 0.1635, 0.1235, 0.1394]

        result = None
        for score in scores:
            result = stabilizer.update(score, 1.0)
            self.assertNotEqual(result, ("Right", "locked"))
        self.assertEqual(stabilizer.current, "Left")

    def test_opposite_scores_do_not_switch_during_same_session(self):
        stabilizer = HandednessStabilizer()
        for _ in range(3):
            result = stabilizer.update(0.1, 0.9)
        self.assertEqual(result, ("Left", "locked"))

        for _ in range(HANDEDNESS_EVIDENCE_WINDOW * 2):
            self.assertEqual(stabilizer.update(0.9, 1.0), ("Left", None))

    def test_short_loss_keeps_label_and_long_loss_forgets_it(self):
        stabilizer = HandednessStabilizer()
        for _ in range(3):
            stabilizer.update(0.1, 0.9)

        for _ in range(HANDEDNESS_FORGET_FRAMES - 1):
            self.assertFalse(stabilizer.mark_missing())
        self.assertEqual(stabilizer.current, "Left")

        self.assertTrue(stabilizer.mark_missing())
        self.assertIsNone(stabilizer.current)

        for _ in range(3):
            result = stabilizer.update(0.9, 0.9)
        self.assertEqual(result, ("Right", "locked"))


class FakeClassifier:
    def __init__(self):
        self.calls = []

    def classify(self, landmarks, handedness, frame_index):
        self.calls.append((landmarks, handedness, frame_index))


class BufferedGestureDispatcherTest(unittest.TestCase):
    def test_replays_warmup_frames_with_locked_handedness(self):
        classifier = FakeClassifier()
        dispatcher = BufferedGestureDispatcher(classifier)

        dispatcher.classify("frame-1", None, 1)
        dispatcher.classify("frame-2", None, 2)
        self.assertEqual(classifier.calls, [])

        dispatcher.classify("frame-3", "Left", 3)
        self.assertEqual(
            classifier.calls,
            [
                ("frame-1", "Left", 1),
                ("frame-2", "Left", 2),
                ("frame-3", "Left", 3),
            ],
        )

    def test_hand_loss_discards_pending_warmup(self):
        classifier = FakeClassifier()
        dispatcher = BufferedGestureDispatcher(classifier)

        dispatcher.classify("old", None, 1)
        dispatcher.classify(None, None, 2)
        dispatcher.classify("new", "Right", 3)

        self.assertEqual(
            classifier.calls,
            [(None, None, 2), ("new", "Right", 3)],
        )


if __name__ == "__main__":
    unittest.main()
