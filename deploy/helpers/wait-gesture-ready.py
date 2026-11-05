#!/usr/bin/env python3
"""Wait for the desktop socket and configured camera before starting Qt."""
import os
from pathlib import Path
import socket
import sys
import time


def ready():
    runtime = os.environ.get('XDG_RUNTIME_DIR', '')
    display = os.environ.get('WAYLAND_DISPLAY', 'wayland-0')
    if not runtime:
        return False
    path = Path(display) if display.startswith('/') else Path(runtime) / display
    camera = os.environ.get('AIR_GESTURE_CAMERA')
    if camera and not Path(camera).exists():
        return False
    try:
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(0.5)
            connection.connect(str(path))
        return True
    except OSError:
        return False


def main():
    deadline = time.monotonic() + 90
    while not ready():
        if time.monotonic() >= deadline:
            print('Gesture startup: desktop or camera is not ready', file=sys.stderr)
            return 1
        time.sleep(0.25)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
