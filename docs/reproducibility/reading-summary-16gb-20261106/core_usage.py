#!/usr/bin/env python3
"""Read /proc/stat and report the two four-core groups; no service changes."""
import argparse
import json
from pathlib import Path
import time


def read():
    rows = {}
    for line in Path("/proc/stat").read_text().splitlines():
        key, *values = line.split()
        if key.startswith("cpu") and key[3:].isdigit():
            v = list(map(int, values[:8]))
            rows[int(key[3:])] = (sum(v), v[3] + v[4])
    return rows


def delta(a, b, cores):
    total = sum(b[i][0] - a[i][0] for i in cores)
    idle = sum(b[i][1] - a[i][1] for i in cores)
    if total <= 0 or not 0 <= idle <= total:
        raise RuntimeError("CPU计数未递增，请检查设备与采样间隔")
    return {"busy_percent": round(100 * (total - idle) / total, 3),
            "total_ticks": total, "busy_ticks": total - idle}


def emit(kind, start, stop, elapsed):
    print(json.dumps({"type": kind, "elapsed_seconds": elapsed,
        "front4": delta(start, stop, range(4)), "rear4": delta(start, stop, range(4, 8))}), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--interval", type=float, default=1, help="采样间隔秒，默认1秒")
    args = parser.parse_args()
    if args.interval <= 0:
        parser.error("interval必须大于0")
    initial = previous = read()
    if not set(range(8)).issubset(initial):
        parser.error("本脚本要求Linux CPU编号0至7可见")
    began = time.monotonic()
    try:
        while True:
            time.sleep(args.interval)
            now = read()
            emit("sample", previous, now, time.monotonic() - began)
            previous = now
    except KeyboardInterrupt:
        emit("summary", initial, read(), time.monotonic() - began)


if __name__ == "__main__":
    main()
