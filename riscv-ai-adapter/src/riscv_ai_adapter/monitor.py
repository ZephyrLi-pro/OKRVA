from __future__ import annotations

import os
import pathlib
import platform
import threading
from typing import Any


def _read_key_values(path: pathlib.Path) -> dict[str, int]:
    result: dict[str, int] = {}
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return result
    for line in lines:
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        fields = value.strip().split()
        if fields and fields[0].isdigit():
            number = int(fields[0])
            if len(fields) > 1 and fields[1].lower() == "kb":
                number *= 1024
            result[key] = number
    return result


def _thermal_zones() -> list[dict[str, Any]]:
    zones: list[dict[str, Any]] = []
    thermal_root = pathlib.Path("/sys/class/thermal")
    for zone in sorted(thermal_root.glob("thermal_zone*")):
        try:
            zone_type = (zone / "type").read_text().strip()
            raw_temp = int((zone / "temp").read_text().strip())
        except (OSError, ValueError):
            continue
        zones.append(
            {
                "name": zone_type,
                "temperature_celsius": raw_temp / 1000.0 if raw_temp > 1000 else float(raw_temp),
            }
        )
    return zones


class SystemMonitor:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._last_cpu: tuple[int, int] | None = None

    def _cpu_percent(self) -> float | None:
        try:
            first = pathlib.Path("/proc/stat").read_text().splitlines()[0].split()
            values = [int(value) for value in first[1:]]
        except (OSError, ValueError, IndexError):
            return None
        idle = values[3] + (values[4] if len(values) > 4 else 0)
        total = sum(values)
        with self._lock:
            previous = self._last_cpu
            self._last_cpu = (total, idle)
        if previous is None:
            return None
        total_delta = total - previous[0]
        idle_delta = idle - previous[1]
        if total_delta <= 0:
            return None
        return max(0.0, min(100.0, 100.0 * (total_delta - idle_delta) / total_delta))

    def snapshot(self, process_ids: dict[str, int] | None = None) -> dict[str, Any]:
        memory = _read_key_values(pathlib.Path("/proc/meminfo"))
        process_memory: dict[str, dict[str, int]] = {}
        for name, pid in (process_ids or {}).items():
            status = _read_key_values(pathlib.Path(f"/proc/{pid}/status"))
            if status:
                process_memory[name] = {
                    "pid": pid,
                    "rss_bytes": status.get("VmRSS", 0),
                    "peak_rss_bytes": status.get("VmHWM", 0),
                    "threads": status.get("Threads", 0),
                }
        try:
            load_1, load_5, load_15 = os.getloadavg()
        except (AttributeError, OSError):
            load_1 = load_5 = load_15 = 0.0
        try:
            affinity = sorted(os.sched_getaffinity(0))
        except AttributeError:
            affinity = list(range(os.cpu_count() or 1))
        return {
            "architecture": platform.machine(),
            "cpu_count": os.cpu_count() or 1,
            "cpu_affinity": affinity,
            "cpu_usage_percent": self._cpu_percent(),
            "load_average": {"1m": load_1, "5m": load_5, "15m": load_15},
            "memory": {
                "total_bytes": memory.get("MemTotal", 0),
                "available_bytes": memory.get("MemAvailable", 0),
                "swap_total_bytes": memory.get("SwapTotal", 0),
                "swap_free_bytes": memory.get("SwapFree", 0),
            },
            "thermal_zones": _thermal_zones(),
            "processes": process_memory,
        }

    @staticmethod
    def render_prometheus(snapshot: dict[str, Any]) -> str:
        memory = snapshot["memory"]
        load = snapshot["load_average"]
        lines = [
            "# HELP rvai_system_memory_bytes System memory by state.",
            "# TYPE rvai_system_memory_bytes gauge",
            f'rvai_system_memory_bytes{{state="total"}} {memory["total_bytes"]}',
            f'rvai_system_memory_bytes{{state="available"}} {memory["available_bytes"]}',
            f'rvai_system_memory_bytes{{state="swap_total"}} {memory["swap_total_bytes"]}',
            f'rvai_system_memory_bytes{{state="swap_free"}} {memory["swap_free_bytes"]}',
            "# HELP rvai_system_load_average System load average.",
            "# TYPE rvai_system_load_average gauge",
            f'rvai_system_load_average{{period="1m"}} {load["1m"]}',
            f'rvai_system_load_average{{period="5m"}} {load["5m"]}',
            f'rvai_system_load_average{{period="15m"}} {load["15m"]}',
        ]
        cpu_percent = snapshot.get("cpu_usage_percent")
        if cpu_percent is not None:
            lines.extend(
                [
                    "# HELP rvai_system_cpu_usage_percent Aggregate CPU utilization.",
                    "# TYPE rvai_system_cpu_usage_percent gauge",
                    f"rvai_system_cpu_usage_percent {cpu_percent:.3f}",
                ]
            )
        for item in snapshot["thermal_zones"]:
            name = str(item["name"]).replace('"', "_")
            lines.append(
                f'rvai_thermal_celsius{{zone="{name}"}} {item["temperature_celsius"]:.3f}'
            )
        for name, process in snapshot["processes"].items():
            safe_name = name.replace('"', "_")
            lines.append(
                f'rvai_backend_process_rss_bytes{{backend="{safe_name}"}} {process["rss_bytes"]}'
            )
        return "\n".join(lines) + "\n"
