#!/usr/bin/env python3
"""Wait for the current user's Wayland desktop and local KylinBot Gateway."""
import os
from pathlib import Path
import socket
import time


def wait_ready(runtime=None, display=None, timeout=120):
    runtime=Path(runtime or os.environ.get('XDG_RUNTIME_DIR',f'/run/user/{os.getuid()}'))
    display=display or os.environ.get('WAYLAND_DISPLAY','wayland-0')
    wayland=Path(display) if Path(display).is_absolute() else runtime/display
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        try:
            if not wayland.exists():raise OSError('Wayland not ready')
            with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as peer:
                peer.settimeout(min(2,max(.01,end-time.monotonic())))
                peer.connect(str(runtime/'kylin-bot/gateway.sock'))
                peer.sendall(b'GET /health HTTP/1.0\r\nHost: localhost\r\n\r\n')
                header=b''
                while b'\r\n' not in header and len(header)<1024:
                    chunk=peer.recv(1024-len(header))
                    if not chunk:break
                    header+=chunk
                status=header.split(b'\r\n',1)[0].split()
                if len(status)<2 or status[1]!=b'200':raise OSError('Gateway not ready')
            return True
        except OSError:
            time.sleep(min(.25,max(0,end-time.monotonic())))
    return False


if __name__=='__main__':
    if not wait_ready():raise SystemExit('Desktop or KylinBot Gateway not ready')
