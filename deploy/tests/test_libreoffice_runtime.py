"""Runtime selection checks never start real Office or change a system."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO/'deploy/installer'))
from common import InstallError
from deployment import units_for_device
from libreoffice_runtime import check_libreoffice, runtime_settings


class LibreOfficeRuntimeTests(unittest.TestCase):
    def launcher(self, home, output, status=0):
        executable = home/'.local/bin/libreoffice-24.8'
        executable.parent.mkdir(parents=True)
        executable.write_text('#!/bin/sh\n[ "$1" = "--version" ] || exit 99\nprintf "%s\\n" "'+output+'"\nexit '+str(status)+'\n')
        executable.chmod(0o755)
        return executable

    def test_missing_launcher_is_not_replaced_by_system_office(self):
        with tempfile.TemporaryDirectory() as directory, patch.dict('os.environ', {}, clear=True):
            with patch('libreoffice_runtime.subprocess.run') as run:
                with self.assertRaisesRegex(InstallError, '尚未|缺少可执行'):
                    check_libreoffice(Path(directory))
                run.assert_not_called()

    def test_24_2_launcher_cannot_satisfy_the_new_deployment(self):
        with tempfile.TemporaryDirectory() as directory, patch.dict('os.environ', {}, clear=True):
            home = Path(directory)
            self.launcher(home, 'LibreOffice 24.2.7.2 420(Build:2)')
            with self.assertRaisesRegex(InstallError, '24.8或以上'):
                check_libreoffice(home)

    def test_versioned_launcher_is_checked_without_starting_office(self):
        with tempfile.TemporaryDirectory() as directory, patch.dict('os.environ', {}, clear=True):
            home = Path(directory)
            executable = self.launcher(home, 'LibreOffice 24.8.7.2 480(Build:2)')
            result = check_libreoffice(home)
            self.assertEqual(result['launcher'], str(executable))
            self.assertEqual(result['version'], '24.8.7')
            self.assertEqual(result['pipe'], 'rvai_trial_248_1000')
            self.assertTrue(result['version_check_passed'])

    def test_failed_or_unrecognized_version_is_rejected(self):
        for output, status in [('LibreOffice 24.8.7.2', 1), ('Library loading failed', 0)]:
            with self.subTest(output=output), tempfile.TemporaryDirectory() as directory, patch.dict('os.environ', {}, clear=True):
                home = Path(directory)
                self.launcher(home, output, status)
                with self.assertRaises(InstallError):
                    check_libreoffice(home)

    def test_version_probe_is_bounded(self):
        with tempfile.TemporaryDirectory() as directory, patch.dict('os.environ', {}, clear=True):
            home = Path(directory)
            executable = self.launcher(home, 'unused')
            with patch('libreoffice_runtime.subprocess.run', side_effect=subprocess.TimeoutExpired(str(executable), 30)):
                with self.assertRaisesRegex(InstallError, 'TimeoutExpired'):
                    check_libreoffice(home)

    def test_custom_runtime_selection_is_rendered_to_reader_service(self):
        with patch.dict('os.environ', {'RVAI_LIBREOFFICE_LAUNCHER': '/opt/Office 24.8/run',
                                      'RVAI_IMPRESS_PIPE': 'custom_pipe_248'}):
            settings = runtime_settings(Path('/home/test'))
            unit = units_for_device(REPO, Path('/home/test'), '/dev/video9',
                                    Path('/home/test/slides.pptx'))['riscv-ai-reader.service']
            self.assertIn('RVAI_LIBREOFFICE_LAUNCHER='+settings['launcher'], unit)
            self.assertIn('RVAI_IMPRESS_PIPE='+settings['pipe'], unit)
            self.assertIn('TimeoutStartSec=300', unit)
            self.assertIn('KillMode=process', unit)

    def test_unsafe_pipe_or_relative_launcher_is_rejected(self):
        for kwargs in (dict(pipe='bad;urp;pipe'), dict(pipe='bad.pipe'), dict(launcher='relative-office')):
            with self.subTest(kwargs=kwargs), self.assertRaises(InstallError):
                runtime_settings(Path('/home/test'), **kwargs)


if __name__ == '__main__':
    unittest.main()
