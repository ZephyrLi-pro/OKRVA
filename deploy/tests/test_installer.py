"""Installer checks use temporary directories; never run apt, sudo or services."""
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import zipfile

REPO=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(REPO/'deploy/installer'))
from common import InstallError,Runner,extract_members,install_model,unpack,unpack_zip
from deployment import Deployment,check_existing,render_bot,units_for_device
from recipes import safe_apt_plan,prefer_package,development_package,merge_system_sdk
from root_install import allowed


class InstallerTests(unittest.TestCase):
    def test_sdk_overlay_keeps_system_glx_metadata_out(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);system=root/'system';sdk=root/'sdk'
            for p in (system,sdk):(p/'lib/cmake/Qt5Gui').mkdir(parents=True)
            (system/'lib/cmake/Qt5Gui/Qt5Gui_QXcbGlxIntegrationPlugin.cmake').write_text('broken')
            (sdk/'lib/cmake/Qt5Gui/Qt5GuiConfig.cmake').write_text('gles')
            (system/'lib/runtime.so').write_text('library')
            merge_system_sdk(system,sdk)
            self.assertFalse((sdk/'lib/cmake/Qt5Gui/Qt5Gui_QXcbGlxIntegrationPlugin.cmake').exists())
            self.assertEqual((sdk/'lib/runtime.so').read_text(),'library')

    def test_apt_removals_are_refused(self):
        safe_apt_plan('Inst cmake [1] (2)\n')
        with self.assertRaises(InstallError):safe_apt_plan('Remv libqt5gui5-gles [5.15]\n')

    def test_gles_alternatives_and_private_development_packages(self):
        self.assertEqual(prefer_package(['qtbase5-dev','qtbase5-gles-dev']),'qtbase5-gles-dev')
        self.assertEqual(prefer_package(['libqt5gui5t64','libqt5gui5-gles']),'libqt5gui5-gles')
        self.assertEqual(prefer_package(['libc-dev']),'libc6-dev')
        self.assertEqual(prefer_package(['libqt5quick5']),'libqt5quick5-gles')
        self.assertTrue(development_package('libkf5wayland-dev'))
        self.assertTrue(development_package('qt5-qmake-bin'))
        self.assertFalse(development_package('libqt5gui5-gles'))

    def test_tar_rejected_before_any_member_is_written(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);archive=root/'bad.tar';dest=root/'out'
            with tarfile.open(archive,'w') as f:
                for name in ('good.txt','../escape.txt'):
                    item=tarfile.TarInfo(name);item.size=1;f.addfile(item,io.BytesIO(b'x'))
            with self.assertRaises(InstallError):unpack(archive,dest)
            self.assertFalse(dest.exists());self.assertFalse((root/'escape.txt').exists())

    def test_tar_link_cannot_escape(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);archive=root/'link.tar'
            with tarfile.open(archive,'w') as f:
                item=tarfile.TarInfo('a/link');item.type=tarfile.SYMTYPE;item.linkname='../../elsewhere';f.addfile(item)
            with self.assertRaises(InstallError):unpack(archive,root/'out')

    def test_zip_rejects_traversal_before_extracting(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);archive=root/'a.zip'
            with zipfile.ZipFile(archive,'w') as f:f.writestr('good','a');f.writestr('../bad','b')
            with self.assertRaises(InstallError):unpack_zip(archive,root/'out')
            self.assertFalse((root/'out').exists())

    def test_archive_member_checksum_is_enforced(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);archive=root/'model.tar';expected=hashlib.sha256(b'correct').hexdigest()
            with tarfile.open(archive,'w') as f:
                item=tarfile.TarInfo('model/model.onnx');item.size=5;f.addfile(item,io.BytesIO(b'wrong'))
            with self.assertRaises(InstallError):extract_members(archive,[dict(member='model.onnx',name='model.onnx',sha256=expected)],root/'out')
            self.assertFalse((root/'out/model.onnx').exists())

    def test_different_existing_model_is_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/'new';dest=root/'existing';source.write_bytes(b'valid');dest.write_bytes(b'mine')
            with self.assertRaises(InstallError):install_model(source,dest,hashlib.sha256(b'valid').hexdigest())
            self.assertEqual(dest.read_bytes(),b'mine')

    def test_render_uses_current_uid_and_localized_directories(self):
        import tomllib
        home=Path('/home/test-user');runtime=Path('/run/user/2001');docs=home/'Documents';desktop=home/'桌面'
        raw=render_bot((REPO/'deploy/templates/kylinbot.toml.in').read_text(),home,runtime,docs,desktop,'asr-secret','gateway-secret')
        data=tomllib.loads(raw)
        self.assertEqual(data['gateway']['socket_path'],str(runtime/'kylin-bot/gateway.sock'))
        self.assertEqual(data['gateway']['paired_tokens'],[hashlib.sha256(b'gateway-secret').hexdigest()])
        self.assertNotIn('gateway-secret',raw)
        self.assertEqual(data['autonomy']['allowed_roots'],[str(docs),str(desktop)])
        self.assertIn(str(docs),data['mcp']['servers'][1]['args'])
        servers={item['name']:item for item in data['mcp']['servers']}
        self.assertEqual(servers['ppt']['tool_timeout_secs'],600)
        self.assertNotIn('--default-presentation',servers['ppt']['args'])
        self.assertEqual(servers['sys']['tool_timeout_secs'],25)
        self.assertEqual(servers['docs']['tool_timeout_secs'],300)

    def test_device_units_do_not_keep_old_camera_or_demo_path(self):
        units=units_for_device(REPO,Path('/home/new'),'/dev/video9',Path('/home/new/My Slides.pptx'),'wayland-2')
        self.assertIn('AIR_GESTURE_CAMERA=/dev/video9',units['ukui-air-gesture.service'])
        self.assertNotIn('Sunplus',units['ukui-air-gesture.service'])
        self.assertIn('WAYLAND_DISPLAY=wayland-2',units['okrva-aiassistant.service'])
        self.assertNotIn('--file',units['riscv-ai-reader.service'])
        self.assertNotIn('My Slides.pptx',units['riscv-ai-reader.service'])
        self.assertNotIn('scenario5-20261028',units['riscv-ai-reader.service'])
        self.assertIn('KillMode=process',units['riscv-ai-reader.service'])
        self.assertIn('RVAI_LIBREOFFICE_LAUNCHER=/home/new/.local/bin/libreoffice-24.8',units['riscv-ai-reader.service'])
        self.assertIn('RVAI_IMPRESS_PIPE=rvai_trial_248_1000',units['riscv-ai-reader.service'])
        self.assertIn('TimeoutStartSec=300',units['riscv-ai-reader.service'])
        self.assertNotIn('KillMode=control-group',units['riscv-ai-reader.service'])

    def test_foreign_config_is_not_adopted(self):
        with tempfile.TemporaryDirectory() as tmp:
            home=Path(tmp);p=home/'.kylinbot/config.toml';p.parent.mkdir();p.write_text('mine')
            with self.assertRaises(InstallError):check_existing(home)
            self.assertEqual(p.read_text(),'mine')

    def test_config_receipt_survives_partial_install_and_preserves_edits(self):
        with tempfile.TemporaryDirectory() as tmp:
            home=Path(tmp);c=SimpleNamespace(work=home/'work',runner=object())
            with patch('deployment.Path.home',return_value=home):
                deploy=Deployment(c,object());p=home/'.kylinbot/config.toml'
                self.assertTrue(deploy.managed(p,'first',0o600))
                check_existing(home)
                self.assertEqual(p.stat().st_mode & 0o777,0o600)
                self.assertFalse(deploy.managed(p,'first',0o600))
                p.write_text('user edit')
                self.assertFalse(deploy.managed(p,'new default',0o600))
                self.assertEqual(p.read_text(),'user edit')

    def test_root_install_paths_are_constrained(self):
        self.assertTrue(allowed('usr/bin/voice-assistant'))
        self.assertTrue(allowed('usr/local/share/ukui-air-gesture/python/main.py'))
        for name in ('../etc/passwd','/etc/passwd','etc/passwd','home/user/.ssh/authorized_keys'):
            self.assertFalse(allowed(name))

    def test_dry_run_does_not_create_work_directory(self):
        with tempfile.TemporaryDirectory() as tmp:
            target=Path(tmp)/'not-created'
            result=subprocess.run([sys.executable,str(REPO/'deploy/installer/install.py'),'--dry-run','--work-dir',str(target)],text=True,capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('只读计划',result.stdout)
            self.assertFalse(target.exists())

    def test_locked_downloads_use_checksums_and_https(self):
        lock=json.loads((REPO/'deploy/installer/lock.json').read_text())
        for item in [*lock['sdk_packages'],lock['rust'],lock['sherpa'],lock['python_pptx'],lock['reading_model_input']]:
            self.assertTrue(item['url'].startswith('https://'))
            self.assertRegex(item['sha256'],r'^[0-9a-f]{64}$')

    def test_http_download_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(InstallError):Runner(Path(tmp)/'work').download('http://untrusted/file')
            self.assertFalse((Path(tmp)/'work').exists())


if __name__=='__main__':unittest.main()
