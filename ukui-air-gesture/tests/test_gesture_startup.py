import importlib.util
import os
from pathlib import Path
import socket
import tempfile
import unittest
from unittest.mock import patch

root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('gesture_startup',root/'deploy/helpers/wait-gesture-ready.py')
startup=importlib.util.module_from_spec(spec);spec.loader.exec_module(startup)


class StartupTests(unittest.TestCase):
    def test_waits_for_connectable_desktop_and_configured_camera(self):
        with tempfile.TemporaryDirectory() as directory:
            camera=Path(directory)/'camera';camera.touch()
            env={'XDG_RUNTIME_DIR':directory,'WAYLAND_DISPLAY':'wayland-test','AIR_GESTURE_CAMERA':str(camera)}
            with patch.dict(os.environ,env):
                self.assertFalse(startup.ready())
                with socket.socket(socket.AF_UNIX) as listener:
                    listener.bind(str(Path(directory)/'wayland-test'))
                    self.assertFalse(startup.ready(),'A stale socket file is not a ready compositor')
                    listener.listen(5)
                    self.assertTrue(startup.ready())
                    camera.unlink()
                    self.assertFalse(startup.ready())


if __name__=='__main__':unittest.main()
