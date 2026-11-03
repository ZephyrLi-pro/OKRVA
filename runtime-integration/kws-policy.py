#!/usr/bin/env python3
"""Fail-open KWS policy. Reads loopback gauges/UI status, never chat or audio."""
import argparse
import json
import math
import os
from pathlib import Path
import signal
import stat
import subprocess
import threading
import time
import urllib.request
import urllib.error

MODEL_PORTS = frozenset({'18082', '18084'})


def managed_llm_command(argv):
    return any(a == '--port' and i + 1 < len(argv) and argv[i + 1] in MODEL_PORTS
               for i, a in enumerate(argv))


def adapter_busy(payload):
    if not isinstance(payload, dict) or not isinstance(payload.get('scheduler'), dict):
        return None
    group = payload['scheduler'].get('group:heavy_inference')
    if not isinstance(group, dict):
        return None
    active, queued = group.get('active'), group.get('queued')
    if any(type(v) is not int or v < 0 for v in (active, queued)):
        return None
    return active > 0 or queued > 0


def llm_busy(payload):
    names = {'llamacpp:requests_processing', 'llamacpp:requests_deferred'}
    values = {}
    for line in payload.splitlines():
        fields = line.split()
        if fields and fields[0] in names:
            if len(fields) != 2 or fields[0] in values:
                return None
            try:
                value = float(fields[1])
            except ValueError:
                return None
            if not math.isfinite(value) or value < 0:
                return None
            values[fields[0]] = value
    return any(value > 0 for value in values.values()) if values.keys() == names else None


def ui_busy(payload):
    if not payload.startswith('s '):
        return None
    try:
        value = json.loads(payload[2:])
    except (ValueError, TypeError):
        return None
    if not isinstance(value, str) or value not in {'ready', 'speaking', 'closing', 'muted', 'capturing', 'full', 'unavailable'}:
        return None
    return value == 'speaking'


class BusyLatch:
    def __init__(self, cooldown=0.3):
        self.until = 0.0
        self.cooldown = cooldown

    def update(self, now, model, ui, cpu=None, scheduler=None):
        # Lost endpoints must never leave voice wake permanently disabled.
        if model is True or ui is True or cpu is True or scheduler is True:
            self.until = now + self.cooldown
            return True
        return now < self.until


class CpuGuard:
    """Read only aggregate ticks of the exact loopback LLM, not its text/env.

    llama.cpp's metrics may queue behind long prefill. CPU activity provides
    an early hint during that interval, independent of the server event loop.
    """
    def __init__(self):
        self.pid = None
        self.previous = None
        self.next_scan = 0.0

    @staticmethod
    def classify(previous, current):
        if previous is None or previous[0] != current[0] or current[2] <= previous[2]:
            return None
        delta = current[1] - previous[1]
        if delta < 0:
            return None
        # 100% is one core. Idle server traffic is well below 20%.
        return delta / os.sysconf('SC_CLK_TCK') / (current[2] - previous[2]) > 0.2

    def read(self):
        now = time.monotonic()
        if self.pid is None:
            if now < self.next_scan:
                return None
            self.next_scan = now + 5
            matches = []
            for path in Path('/proc').iterdir():
                if not path.name.isdigit():
                    continue
                try:
                    if (path / 'exe').resolve(strict=True).name != 'llama-server':
                        continue
                    argv = (path / 'cmdline').read_bytes().decode().strip('\0').split('\0')
                    if managed_llm_command(argv):
                        matches.append(int(path.name))
                except (OSError, ValueError):
                    continue
            if len(matches) != 1:
                return None
            self.pid = matches[0]
        try:
            fields = Path(f'/proc/{self.pid}/stat').read_text().rsplit(')', 1)[1].split()
            current = (fields[19], int(fields[11]) + int(fields[12]), now)
        except (OSError, ValueError, IndexError):
            self.pid = None
            self.previous = None
            return None
        busy = self.classify(self.previous, current)
        if self.previous and self.previous[0] != current[0]:
            self.pid = None
            self.previous = None
            return None
        self.previous = current
        return busy


class StateFile:
    def __init__(self, path):
        self.path = Path(path)
        parent = self.path.parent.lstat()
        if not stat.S_ISDIR(parent.st_mode) or parent.st_uid != os.getuid() or parent.st_mode & 0o077:
            raise RuntimeError('KWS policy needs an owned, private runtime directory')
        try:
            target = self.path.lstat()
        except FileNotFoundError:
            pass
        else:
            if not stat.S_ISREG(target.st_mode) or target.st_uid != os.getuid() or target.st_mode & 0o077:
                raise RuntimeError('Refusing unsafe KWS policy state path')
        self.sequence = 0

    def write(self, busy, timestamp=None):
        self.sequence += 1
        temporary = self.path.with_name(f'.state-{os.getpid()}-{self.sequence}')
        fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        try:
            with os.fdopen(fd, 'w') as stream:
                stream.write(f'{int(time.time() if timestamp is None else timestamp)} {int(busy)}\n')
            os.replace(temporary, self.path)
        finally:
            temporary.unlink(missing_ok=True)


def read_model():
    try:
        with urllib.request.urlopen('http://127.0.0.1:18082/metrics', timeout=0.3) as response:
            data = response.read(65537)
        return llm_busy(data.decode('ascii')) if len(data) <= 65536 else None
    except (OSError, ValueError):
        return None


def read_ui():
    try:
        result = subprocess.run(
            ['busctl', '--user', '--auto-start=no', 'call', 'org.ukui.voice_assistant',
             '/org/ukui/voice_assistant', 'org.ukui.voice_assistant', 'GetInputStatus'],
            timeout=0.35, capture_output=True, text=True, check=False)
        return ui_busy(result.stdout.strip()) if result.returncode == 0 else None
    except (OSError, subprocess.TimeoutExpired):
        return None


def read_adapter():
    try:
        try:
            response = urllib.request.urlopen('http://127.0.0.1:18080/health', timeout=0.3)
        except urllib.error.HTTPError as error:
            if error.code != 503:
                return None
            response = error
        with response:
            data = response.read(65537)
        return adapter_busy(json.loads(data)) if len(data) <= 65536 else None
    except (OSError, ValueError):
        return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--state', type=Path, required=True)
    args = parser.parse_args()
    state, latch, cpu, stop = StateFile(args.state), BusyLatch(), CpuGuard(), threading.Event()
    for signum in (signal.SIGINT, signal.SIGTERM):
        signal.signal(signum, lambda *_: stop.set())
    state.write(False)
    print('KWS policy watcher ready (both managed LLMs + Adapter activity + UI playback)', flush=True)
    try:
        while not stop.is_set():
            started = time.monotonic()
            model, ui = read_model(), read_ui()
            busy = latch.update(time.monotonic(), model, ui, cpu.read(), read_adapter())
            state.write(busy)
            stop.wait(max(0.01, 0.25 - (time.monotonic() - started)))
    finally:
        state.write(False)


if __name__ == '__main__':
    main()
