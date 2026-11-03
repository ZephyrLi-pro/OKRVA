"""Bound one owned inference process by available RAM; never stop other services."""
from __future__ import annotations

import argparse
import signal
import subprocess
import threading
from pathlib import Path


def available_mib():
    for line in Path('/proc/meminfo').read_text().splitlines():
        if line.startswith('MemAvailable:'):
            return int(line.split()[1]) / 1024
    raise RuntimeError('MemAvailable is unavailable')


def supervise(command, *, startup_min_mib=2048, runtime_min_mib=512,
              available=available_mib, interval=0.2):
    free = available()
    if free < startup_min_mib:
        raise RuntimeError(f'Not enough available RAM to load the optional model: {free:.0f} MiB available, {startup_min_mib} MiB required')
    stop = threading.Event()
    old_handlers = {}
    for sig in (signal.SIGTERM, signal.SIGINT):
        old_handlers[sig] = signal.signal(sig, lambda *_: stop.set())
    child = None
    try:
        child = subprocess.Popen(command)
        while child.poll() is None and not stop.wait(interval):
            free = available()
            if free < runtime_min_mib:
                raise RuntimeError(f'Optional model stopped by the RAM guard: {free:.0f} MiB available, {runtime_min_mib} MiB minimum')
        return child.returncode if child.poll() is not None else 143
    finally:
        if child is not None and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=1)
        for sig, handler in old_handlers.items():
            signal.signal(sig, handler)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--startup-min-mib', type=int, default=2048)
    parser.add_argument('--runtime-min-mib', type=int, default=512)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command or not 0 < args.runtime_min_mib < args.startup_min_mib:
        parser.error('A command and positive startup > runtime memory limits are required')
    return supervise(command, startup_min_mib=args.startup_min_mib,
                     runtime_min_mib=args.runtime_min_mib)


if __name__ == '__main__':
    raise SystemExit(main())
