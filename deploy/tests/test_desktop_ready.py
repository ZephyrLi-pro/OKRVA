import importlib.util
from pathlib import Path
import socket
import tempfile
import threading
import unittest

spec=importlib.util.spec_from_file_location('okrva_ready',Path(__file__).resolve().parents[1]/'helpers/wait-ready.py')
ready=importlib.util.module_from_spec(spec);spec.loader.exec_module(ready)


class DesktopReadyTests(unittest.TestCase):
    def test_current_runtime_and_nondefault_display(self):
        with tempfile.TemporaryDirectory() as tmp:
            runtime=Path(tmp);(runtime/'wayland-3').touch();(runtime/'kylin-bot').mkdir()
            with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as server:
                server.bind(str(runtime/'kylin-bot/gateway.sock'));server.listen(1)
                def reply():
                    peer,_=server.accept()
                    with peer:
                        self.assertIn(b'GET /health',peer.recv(1024))
                        peer.sendall(b'HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n')
                worker=threading.Thread(target=reply,daemon=True);worker.start()
                self.assertTrue(ready.wait_ready(runtime,'wayland-3',timeout=1))
                worker.join(1);self.assertFalse(worker.is_alive())

    def test_missing_desktop_times_out(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertFalse(ready.wait_ready(tmp,'missing',timeout=.02))

    def test_missing_gateway_times_out(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp)/'wayland-0').touch()
            self.assertFalse(ready.wait_ready(tmp,'wayland-0',timeout=.02))


if __name__=='__main__':unittest.main()
