import itertools
import os
import types
import unittest
from unittest.mock import Mock, patch

from office_reader import activate_frame


class WindowActivationTests(unittest.TestCase):
    def frame(self):
        frame=Mock()
        window=frame.getContainerWindow.return_value
        window.IsMinimized=False
        window.getAccessibleContext.return_value.getAccessibleName.return_value='原始PPT.pptx — LibreOffice Impress'
        return frame,window

    def test_wayland_restores_only_observed_impress_window_and_confirms_state(self):
        frame,window=self.frame()
        with patch.dict(os.environ, {'WAYLAND_DISPLAY':'wayland-0'}), \
                patch('office_reader.Path.is_file',return_value=True), \
                patch('office_reader.subprocess.run',return_value=types.SimpleNamespace(returncode=0,stderr='')) as run:
            self.assertIs(activate_frame(frame),window)
        self.assertEqual(run.call_count,2)
        self.assertEqual(run.call_args_list[0].args[0],['/usr/bin/wlrctl','toplevel','focus',
            'app_id:libreoffice-impress','title:原始PPT.pptx — LibreOffice Impress'])
        self.assertEqual(run.call_args_list[1].args[0][-1],'state:unminimized')

    def test_false_native_minimized_flag_does_not_override_hidden_compositor_state(self):
        frame,window=self.frame()
        ticks=itertools.count(0,1)
        def run(command,**kwargs):
            return types.SimpleNamespace(returncode=0 if command[2]=='focus' else 1,stderr='')
        with patch.dict(os.environ, {'WAYLAND_DISPLAY':'wayland-0'}), \
                patch('office_reader.Path.is_file',return_value=True), \
                patch('office_reader.subprocess.run',side_effect=run), \
                patch('office_reader.time.monotonic',side_effect=lambda:next(ticks)), \
                patch('office_reader.time.sleep'):
            with self.assertRaisesRegex(RuntimeError,'Wayland'):activate_frame(frame)

    def test_failed_exact_window_focus_is_not_acknowledged(self):
        frame,window=self.frame()
        with patch.dict(os.environ, {'WAYLAND_DISPLAY':'wayland-0'}), \
                patch('office_reader.Path.is_file',return_value=True), \
                patch('office_reader.subprocess.run',return_value=types.SimpleNamespace(returncode=1,stderr='')):
            with self.assertRaises(RuntimeError):activate_frame(frame)

    def test_non_wayland_native_window_uses_regular_activation(self):
        frame,window=self.frame()
        with patch.dict(os.environ, {'WAYLAND_DISPLAY':''}), \
                patch('office_reader.subprocess.run') as run:
            self.assertIs(activate_frame(frame),window)
        run.assert_not_called()
        window.toFront.assert_called_once()


if __name__=='__main__':unittest.main()
