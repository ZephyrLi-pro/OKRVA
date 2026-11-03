from __future__ import annotations

import json
import contextlib
import pathlib
import socket
import sys
import threading
import unittest
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "src"))

from riscv_ai_adapter.config import AdapterConfig, BackendConfig, ServerConfig
from riscv_ai_adapter.backends import UpstreamResponse
from riscv_ai_adapter.server import make_server


class StreamingTest(unittest.TestCase):
    def test_mid_stream_timeout_does_not_append_a_second_http_response(self) -> None:
        first = b'data: {"choices":[{"delta":{"content":"hello"}}]}\n\n'
        released = threading.Event()

        class FailingStream:
            count = 0

            def read(self, _size: int) -> bytes:
                self.count += 1
                if self.count == 1:
                    return first
                raise TimeoutError("synthetic upstream timeout")

        @contextlib.contextmanager
        def fake_open(*_args: object):
            try:
                yield UpstreamResponse(200, {"Content-Type": "text/event-stream"}, FailingStream())
            finally:
                released.set()

        config = AdapterConfig(server=ServerConfig(port=0), resource_groups={},
            backends=(BackendConfig(name="stream", capability="chat",
                url="http://127.0.0.1:1/v1/chat/completions"),))
        adapter = make_server(config)
        with patch.object(adapter.registry, "open", fake_open):
            threading.Thread(target=adapter.serve_forever, daemon=True).start()
            try:
                body = b'{"model":"test","messages":[],"stream":true}'
                with socket.create_connection(("127.0.0.1", adapter.server_port), timeout=2) as client:
                    client.sendall(b"POST /v1/chat/completions HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: "
                        + str(len(body)).encode() + b"\r\n\r\n" + body)
                    chunks = []
                    while chunk := client.recv(4096):
                        chunks.append(chunk)
                wire = b"".join(chunks)
                self.assertTrue(wire.startswith(b"HTTP/1.1 200"))
                self.assertIn(first, wire)
                self.assertEqual(wire.count(b"HTTP/1.1 "), 1)
                self.assertNotIn(b"backend_transport_error", wire)
                self.assertFalse(wire.endswith(b"0\r\n\r\n"))
                self.assertTrue(released.wait(1))
            finally:
                adapter.shutdown()
                adapter.server_close()

    def test_first_event_arrives_before_upstream_finishes(self) -> None:
        for fixed_length in (False, True):
            with self.subTest(content_length=fixed_length):
                self.check_stream(fixed_length)

    def check_stream(self, fixed_length: bool) -> None:
        release = threading.Event()
        first = b'data: {"choices":[{"delta":{"content":"hello"}}]}\n\n'
        last = b'data: [DONE]\n\n'

        class StreamHandler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *_args: object) -> None:
                pass

            def do_POST(self) -> None:
                self.rfile.read(int(self.headers["Content-Length"]))
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                if fixed_length:
                    self.send_header("Content-Length", str(len(first) + len(last)))
                else:
                    self.send_header("Transfer-Encoding", "chunked")
                self.end_headers()

                def send(body: bytes) -> None:
                    if fixed_length:
                        self.wfile.write(body)
                    else:
                        self.wfile.write(f"{len(body):X}\r\n".encode() + body + b"\r\n")
                    self.wfile.flush()

                send(first)
                # The client releases the final event only after receiving the
                # first one. A buffering proxy deterministically times out.
                release.wait(5)
                send(last)
                if not fixed_length:
                    self.wfile.write(b"0\r\n\r\n")
                    self.wfile.flush()

        backend = ThreadingHTTPServer(("127.0.0.1", 0), StreamHandler)
        threading.Thread(target=backend.serve_forever, daemon=True).start()
        config = AdapterConfig(
            server=ServerConfig(port=0), resource_groups={},
            backends=(BackendConfig(name="stream", capability="chat",
                url=f"http://127.0.0.1:{backend.server_port}/v1/chat/completions"),),
        )
        adapter = make_server(config)
        threading.Thread(target=adapter.serve_forever, daemon=True).start()
        try:
            request = urllib.request.Request(
                f"http://127.0.0.1:{adapter.server_port}/v1/chat/completions",
                data=json.dumps({"model": "test", "messages": [], "stream": True}).encode(),
                headers={"Content-Type": "application/json"},
            )
            with urllib.request.urlopen(request, timeout=2) as response:
                self.assertEqual(response.headers["Content-Type"], "text/event-stream")
                self.assertEqual(response.readline(), first.splitlines(keepends=True)[0])
                release.set()
                self.assertEqual(response.read(), b"\n" + last)
        finally:
            release.set()
            adapter.shutdown()
            adapter.server_close()
            backend.shutdown()
            backend.server_close()


if __name__ == "__main__":
    unittest.main()
