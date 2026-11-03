from __future__ import annotations

import threading
from collections import defaultdict


class Metrics:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._requests: dict[tuple[str, str], int] = defaultdict(int)
        self._errors: dict[str, int] = defaultdict(int)
        self._in_flight: dict[str, int] = defaultdict(int)
        self._latency_seconds: dict[str, float] = defaultdict(float)
        self._queue_wait_seconds: dict[str, float] = defaultdict(float)

    def begin(self, backend: str) -> None:
        with self._lock:
            self._in_flight[backend] += 1

    def finish(self, backend: str, status: int, elapsed_seconds: float) -> None:
        status_class = f"{status // 100}xx" if status else "transport_error"
        with self._lock:
            self._in_flight[backend] = max(0, self._in_flight[backend] - 1)
            self._requests[(backend, status_class)] += 1
            self._latency_seconds[backend] += elapsed_seconds
            if status == 0 or status >= 400:
                self._errors[backend] += 1

    def rejected(self, backend: str) -> None:
        with self._lock:
            self._requests[(backend, "rejected")] += 1

    def observe_queue_wait(self, backend: str, elapsed_seconds: float) -> None:
        with self._lock:
            self._queue_wait_seconds[backend] += elapsed_seconds

    def render_prometheus(self) -> str:
        with self._lock:
            lines = [
                "# HELP rvai_backend_requests_total Requests routed to a backend.",
                "# TYPE rvai_backend_requests_total counter",
            ]
            for (backend, status), value in sorted(self._requests.items()):
                lines.append(
                    f'rvai_backend_requests_total{{backend="{backend}",status="{status}"}} {value}'
                )
            lines.extend(
                [
                    "# HELP rvai_backend_errors_total Backend and transport errors.",
                    "# TYPE rvai_backend_errors_total counter",
                ]
            )
            for backend, value in sorted(self._errors.items()):
                lines.append(f'rvai_backend_errors_total{{backend="{backend}"}} {value}')
            lines.extend(
                [
                    "# HELP rvai_backend_in_flight Current in-flight requests.",
                    "# TYPE rvai_backend_in_flight gauge",
                ]
            )
            for backend, value in sorted(self._in_flight.items()):
                lines.append(f'rvai_backend_in_flight{{backend="{backend}"}} {value}')
            lines.extend(
                [
                    "# HELP rvai_backend_latency_seconds_total Accumulated request latency.",
                    "# TYPE rvai_backend_latency_seconds_total counter",
                ]
            )
            for backend, value in sorted(self._latency_seconds.items()):
                lines.append(
                    f'rvai_backend_latency_seconds_total{{backend="{backend}"}} {value:.6f}'
                )
            lines.extend(
                [
                    "# HELP rvai_backend_queue_wait_seconds_total Time spent waiting for inference resources.",
                    "# TYPE rvai_backend_queue_wait_seconds_total counter",
                ]
            )
            for backend, value in sorted(self._queue_wait_seconds.items()):
                lines.append(
                    f'rvai_backend_queue_wait_seconds_total{{backend="{backend}"}} {value:.6f}'
                )
        return "\n".join(lines) + "\n"
