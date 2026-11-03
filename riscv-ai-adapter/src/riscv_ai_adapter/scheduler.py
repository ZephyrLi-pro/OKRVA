from __future__ import annotations

import contextlib
import heapq
import itertools
import threading
import time
from dataclasses import dataclass, field
from typing import Iterator


class SchedulerTimeoutError(RuntimeError):
    pass


@dataclass(order=True)
class _Waiter:
    sort_priority: int
    sequence: int
    cancelled: bool = field(default=False, compare=False)


class PriorityPool:
    """A bounded resource pool whose waiters are ordered by priority then FIFO."""

    def __init__(self, name: str, limit: int) -> None:
        if limit <= 0:
            raise ValueError("pool limit must be positive")
        self.name = name
        self.limit = limit
        self._active = 0
        self._waiters: list[_Waiter] = []
        self._sequence = itertools.count()
        self._condition = threading.Condition()

    def acquire(self, priority: int, timeout: float) -> float:
        started = time.monotonic()
        waiter = _Waiter(sort_priority=-priority, sequence=next(self._sequence))
        deadline = started + timeout
        with self._condition:
            heapq.heappush(self._waiters, waiter)
            while True:
                self._discard_cancelled_head()
                if self._active < self.limit and self._waiters[0] is waiter:
                    heapq.heappop(self._waiters)
                    self._active += 1
                    return time.monotonic() - started
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    waiter.cancelled = True
                    self._discard_cancelled_head()
                    self._condition.notify_all()
                    raise SchedulerTimeoutError(
                        f"resource pool {self.name!r} did not become available"
                    )
                self._condition.wait(remaining)

    def release(self) -> None:
        with self._condition:
            if self._active <= 0:
                raise RuntimeError(f"resource pool {self.name!r} released too often")
            self._active -= 1
            self._condition.notify_all()

    def snapshot(self) -> dict[str, int]:
        with self._condition:
            queued = sum(not waiter.cancelled for waiter in self._waiters)
            return {"limit": self.limit, "active": self._active, "queued": queued}

    def _discard_cancelled_head(self) -> None:
        while self._waiters and self._waiters[0].cancelled:
            heapq.heappop(self._waiters)


class PriorityScheduler:
    def __init__(
        self,
        backend_limits: dict[str, int],
        resource_group_limits: dict[str, int],
    ) -> None:
        self._backend_pools = {
            name: PriorityPool(f"backend:{name}", limit)
            for name, limit in backend_limits.items()
        }
        self._group_pools = {
            name: PriorityPool(f"group:{name}", limit)
            for name, limit in resource_group_limits.items()
        }

    @contextlib.contextmanager
    def lease(
        self,
        backend_name: str,
        resource_group: str,
        priority: int,
        timeout: float,
    ) -> Iterator[float]:
        group_pool = self._group_pools.get(resource_group)
        backend_pool = self._backend_pools[backend_name]
        group_acquired = False
        backend_acquired = False
        waited = 0.0
        try:
            if group_pool is not None:
                waited += group_pool.acquire(priority, timeout)
                group_acquired = True
                timeout = max(0.0, timeout - waited)
            waited += backend_pool.acquire(priority, timeout)
            backend_acquired = True
            yield waited
        finally:
            if backend_acquired:
                backend_pool.release()
            if group_acquired and group_pool is not None:
                group_pool.release()

    def snapshot(self) -> dict[str, dict[str, int]]:
        result = {
            pool.name: pool.snapshot() for pool in self._group_pools.values()
        }
        result.update(
            {pool.name: pool.snapshot() for pool in self._backend_pools.values()}
        )
        return result
