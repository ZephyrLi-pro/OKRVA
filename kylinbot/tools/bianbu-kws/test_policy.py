import importlib.util
import os
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('policy', Path(__file__).with_name('kws-policy.py'))
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)


class PolicyTests(unittest.TestCase):
    def test_metrics(self):
        idle = '# gauges\nllamacpp:requests_processing 0\nllamacpp:requests_deferred 0\n'
        self.assertIs(policy.llm_busy(idle), False)
        self.assertIs(policy.llm_busy(idle.replace('processing 0', 'processing 1')), True)
        self.assertIs(policy.llm_busy(idle.replace('deferred 0', 'deferred 1')), True)
        for bad in ('', 'error', idle.replace('processing 0', 'processing nan'),
                    idle.replace('processing 0', 'processing -1'), idle + idle):
            self.assertIsNone(policy.llm_busy(bad))

    def test_ui(self):
        self.assertIs(policy.ui_busy('s "speaking"'), True)
        for value in ('ready', 'capturing', 'closing', 'muted', 'full', 'unavailable'):
            self.assertIs(policy.ui_busy(f's "{value}"'), False)
        for bad in ('', 'b true', 's "other"', 's malformed', 's []'):
            self.assertIsNone(policy.ui_busy(bad))

    def test_busy_resume_fail_open(self):
        latch = policy.BusyLatch()
        self.assertFalse(latch.update(10, False, False))
        self.assertTrue(latch.update(10, True, None))
        self.assertTrue(latch.update(10.1, False, False))
        self.assertFalse(latch.update(10.4, None, None))
        self.assertTrue(latch.update(12, None, True))
        self.assertFalse(latch.update(13, None, None))
        self.assertTrue(latch.update(15, None, False, True))
        self.assertFalse(latch.update(16, None, False, None))

    def test_cpu_guard(self):
        hz = os.sysconf('SC_CLK_TCK')
        classify = policy.CpuGuard.classify
        self.assertIsNone(classify(None, ('same', 10, 2)))
        self.assertIsNone(classify(('old', 10, 1), ('new', 10, 2)))
        self.assertIsNone(classify(('same', 10, 2), ('same', 20, 2)))
        self.assertIsNone(classify(('same', 10, 1), ('same', 5, 2)))
        self.assertIs(classify(('same', 0, 1), ('same', hz * 4, 2)), True)
        self.assertIs(classify(('same', 0, 1), ('same', 0, 2)), False)

    def test_atomic_private_state(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'state'
            state = policy.StateFile(path)
            state.write(True, 123)
            self.assertEqual(path.read_text(), '123 1\n')
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            state.write(False, 124)
            self.assertEqual(path.read_text(), '124 0\n')
            self.assertEqual(list(Path(directory).iterdir()), [path])

    def test_unsafe_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'state'
            path.symlink_to(Path(directory) / 'other')
            with self.assertRaises(RuntimeError):
                policy.StateFile(path)
            path.unlink()
            os.chmod(directory, 0o755)
            with self.assertRaises(RuntimeError):
                policy.StateFile(path)


if __name__ == '__main__':
    unittest.main()
