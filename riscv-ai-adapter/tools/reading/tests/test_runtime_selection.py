import os
from pathlib import Path
import unittest
from unittest.mock import patch

from office_reader import runtime_settings


class RuntimeSelectionTests(unittest.TestCase):
    def test_default_targets_formal_248_launcher_and_existing_248_pipe(self):
        with patch.dict(os.environ, {}, clear=True):
            launcher, pipe = runtime_settings()
        self.assertEqual(launcher, str(Path.home()/'.local/bin/libreoffice-24.8'))
        self.assertEqual(pipe, 'rvai_trial_248_'+str(os.getuid()))

    def test_explicit_deployment_selects_custom_native_office(self):
        with patch.dict(os.environ, {'RVAI_LIBREOFFICE_LAUNCHER':'/opt/office/bin/impress',
                                    'RVAI_IMPRESS_PIPE':'presentation_248_custom'}):
            self.assertEqual(runtime_settings(), ('/opt/office/bin/impress','presentation_248_custom'))

    def test_invalid_pipe_cannot_inject_uno_connection_parameters(self):
        with patch.dict(os.environ, {'RVAI_IMPRESS_PIPE':'name;urp;OtherObject'}):
            with self.assertRaises(ValueError):runtime_settings()

    def test_relative_launcher_is_rejected(self):
        with patch.dict(os.environ, {'RVAI_LIBREOFFICE_LAUNCHER':'libreoffice'}):
            with self.assertRaises(ValueError):runtime_settings()


if __name__ == '__main__':unittest.main()
