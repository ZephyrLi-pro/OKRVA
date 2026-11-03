"""No model/hardware required: ABI wrapper and end-to-end cancellation tests."""
import importlib.util
import io
import json
import pathlib
import select
import socket
import sys
import threading
import time
import unittest
import urllib.error
import urllib.request
import wave
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from unittest.mock import Mock, patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
from riscv_ai_adapter.backends import _open_local_speech
from riscv_ai_adapter.config import AdapterConfig, BackendConfig, ServerConfig
from riscv_ai_adapter.server import make_server


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


native, tts = load("tts_native"), load("tts_server")


class NativeCancellation(unittest.TestCase):
    def test_cancel_drains_reply_and_preserves_worker(self):
        engine = native.NativeEngine.__new__(native.NativeEngine)
        engine.backend = "matcha"
        engine.cancellation = threading.Event()
        engine.connection = Mock()
        engine.process = Mock()
        engine.connection.poll.side_effect = [False, True]
        engine.connection.recv_bytes.return_value = b""
        with self.assertRaises(InterruptedError):
            engine.synthesize("one. two.", 0, 1, cancelled=lambda: True)
        self.assertTrue(engine.cancellation.is_set())
        engine.process.terminate.assert_not_called()
        engine.connection.recv_bytes.assert_called_once()
        engine.connection.poll.side_effect = None
        engine.connection.poll.return_value = True
        engine.connection.recv_bytes.return_value = b"next wave"
        self.assertEqual(engine.synthesize("next", 0, 1), b"next wave")
        self.assertFalse(engine.cancellation.is_set())

    def test_alias_and_single_speaker(self):
        for model in ("vits-zh", "matcha-zh-en", "matcha-icefall-zh-en"):
            self.assertEqual(tts.parse_request(json.dumps({"input": "你好。", "model": model}).encode(),
                                               "matcha-zh-en"), ("你好。", 0, 1.0))
        with self.assertRaises(ValueError):
            tts.parse_request(b'{"input":"hello","voice":"zh-1"}', "matcha-zh-en")


class SlowSpeech(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_):
        pass

    def do_POST(self):
        body = self.rfile.read(int(self.headers["Content-Length"]))
        if body == b"wait":
            self.server.started.set()
            deadline = time.monotonic() + 4
            while time.monotonic() < deadline:
                readable, _, _ = select.select([self.connection], [], [], .05)
                if readable and self.connection.recv(1, socket.MSG_PEEK) == b"":
                    self.server.disconnected.set()
                    self.close_connection = True
                    return
        response = b"wave" if body != b"error" else b"busy"
        self.send_response(200 if body != b"error" else 429)
        self.send_header("Content-Length", str(len(response)))
        self.end_headers()
        self.wfile.write(response)


class TransportCancellation(unittest.TestCase):
    def setUp(self):
        self.backend = ThreadingHTTPServer(("127.0.0.1", 0), SlowSpeech)
        self.backend.daemon_threads = True
        self.backend.started, self.backend.disconnected = threading.Event(), threading.Event()
        self.worker = threading.Thread(target=self.backend.serve_forever, daemon=True)
        self.worker.start()
        self.url = f"http://127.0.0.1:{self.backend.server_port}/v1/audio/speech"

    def tearDown(self):
        self.backend.shutdown()
        self.backend.server_close()
        self.worker.join(2)

    def test_success_and_upstream_error(self):
        for body, status in ((b"ok", 200), (b"error", 429)):
            response = _open_local_speech(urllib.request.Request(self.url, data=body), 3, lambda: False)
            try:
                self.assertEqual(response.status, status)
                self.assertEqual(response.read(), b"wave" if status == 200 else b"busy")
            finally:
                response.close()

    def test_cancel_before_connect(self):
        with self.assertRaises(ConnectionResetError):
            _open_local_speech(urllib.request.Request(self.url, data=b"wait"), 3, lambda: True)
        self.assertFalse(self.backend.started.is_set())

    def test_abort_while_waiting_for_headers(self):
        with self.assertRaises(ConnectionResetError):
            _open_local_speech(urllib.request.Request(self.url, data=b"wait"), 3,
                               self.backend.started.is_set)
        self.assertTrue(self.backend.disconnected.wait(1))

    def test_timeout_closes_upstream(self):
        with self.assertRaises((TimeoutError, OSError)):
            _open_local_speech(urllib.request.Request(self.url, data=b"wait"), .2, lambda: False)
        self.assertTrue(self.backend.disconnected.wait(1))

    def test_gui_abort_propagates_through_adapter(self):
        backend = BackendConfig(name="tts", capability="speech", url=self.url, timeout_seconds=3)
        adapter = make_server(AdapterConfig(ServerConfig(port=0), (backend,), {}))
        worker = threading.Thread(target=adapter.serve_forever, daemon=True)
        worker.start()
        try:
            client = socket.create_connection(adapter.server_address, timeout=2)
            client.sendall(b"POST /v1/audio/speech HTTP/1.1\r\nHost: localhost\r\n"
                           b"Content-Type: application/json\r\nContent-Length: 4\r\n\r\nwait")
            self.assertTrue(self.backend.started.wait(1))
            client.close()
            self.assertTrue(self.backend.disconnected.wait(1))
            deadline = time.monotonic() + 1
            while time.monotonic() < deadline:
                if not adapter.registry.scheduler.snapshot()["backend:tts"]["active"]:
                    break
                time.sleep(.02)
            self.assertEqual(adapter.registry.scheduler.snapshot()["backend:tts"]["active"], 0)
            request = urllib.request.Request(f"http://127.0.0.1:{adapter.server_port}/v1/audio/speech", data=b"ok")
            with urllib.request.urlopen(request, timeout=2) as reply:
                self.assertEqual(reply.read(), b"wave")
        finally:
            adapter.shutdown()
            adapter.server_close()
            worker.join(2)


if __name__ == "__main__":
    unittest.main()
