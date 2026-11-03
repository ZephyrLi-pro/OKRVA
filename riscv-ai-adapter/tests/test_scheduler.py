from __future__ import annotations

import pathlib
import sys
import threading
import time
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from riscv_ai_adapter.backends import ProcessManager  # noqa: E402
from riscv_ai_adapter.config import BackendConfig  # noqa: E402
from riscv_ai_adapter.monitor import SystemMonitor  # noqa: E402
from riscv_ai_adapter.scheduler import PriorityPool, SchedulerTimeoutError  # noqa: E402


class PriorityPoolTest(unittest.TestCase):
    def test_higher_priority_waiter_runs_first(self) -> None:
        pool = PriorityPool("test", 1)
        pool.acquire(priority=50, timeout=1)
        order: list[str] = []

        def worker(name: str, priority: int) -> None:
            pool.acquire(priority=priority, timeout=2)
            order.append(name)
            time.sleep(0.02)
            pool.release()

        low = threading.Thread(target=worker, args=("low", 10))
        high = threading.Thread(target=worker, args=("high", 90))
        low.start()
        high.start()
        deadline = time.monotonic() + 1
        while pool.snapshot()["queued"] < 2 and time.monotonic() < deadline:
            time.sleep(0.005)
        pool.release()
        low.join(timeout=2)
        high.join(timeout=2)
        self.assertEqual(order, ["high", "low"])

    def test_timeout_removes_waiter(self) -> None:
        pool = PriorityPool("test", 1)
        pool.acquire(priority=50, timeout=1)
        with self.assertRaises(SchedulerTimeoutError):
            pool.acquire(priority=50, timeout=0.01)
        self.assertEqual(pool.snapshot()["queued"], 0)
        pool.release()


class ProcessManagerTest(unittest.TestCase):
    def test_managed_process_can_start_and_stop(self) -> None:
        backend = BackendConfig(
            name="worker",
            capability="chat",
            url="http://127.0.0.1:9/v1/chat/completions",
            managed=True,
            command=(sys.executable, "-c", "import time; time.sleep(30)"),
        )
        manager = ProcessManager((backend,))
        try:
            manager.start_one("worker")
            snapshot = manager.snapshot()[0]
            self.assertTrue(snapshot["running"])
            self.assertIsInstance(snapshot["pid"], int)
            manager.stop_one("worker")
            self.assertFalse(manager.snapshot()[0]["running"])
        finally:
            manager.stop()


class SystemMonitorTest(unittest.TestCase):
    def test_snapshot_has_portable_core_fields(self) -> None:
        snapshot = SystemMonitor().snapshot()
        self.assertTrue(snapshot["architecture"])
        self.assertGreater(snapshot["cpu_count"], 0)
        self.assertIn("total_bytes", snapshot["memory"])
        rendered = SystemMonitor.render_prometheus(snapshot)
        self.assertIn("rvai_system_memory_bytes", rendered)


if __name__ == "__main__":
    unittest.main()
