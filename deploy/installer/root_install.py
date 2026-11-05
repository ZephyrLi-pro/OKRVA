#!/usr/bin/env python3
"""Install a checked build staging tree into a fixed set of application paths."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import stat
import tempfile


EXACT={
 'usr/bin/kylin-aiassistant','usr/bin/voice-assistant',
 'usr/local/bin/kylinbot','usr/local/bin/ukui-air-gesture',
 'usr/share/dbus-1/services/org.ukui.kylin_aiassistant.service',
 'usr/share/dbus-1/services/org.ukui.voice_assistant.service',
 'usr/share/applications/kylin-aiassistant.desktop','usr/share/applications/voice-assistant.desktop',
 'usr/share/pixmaps/AI-Writing.svg','usr/share/pixmaps/AI-Writing.png',
 'etc/kylin-aiassistant/voice-assistant.ini',
 'etc/ukui/ukui-notification/default-app/kylin-aiassistant.json',
 'etc/udev/rules.d/70-okrva-tcm.rules','etc/udev/rules.d/99-zz-okrva-uinput.rules',
 'usr/share/glib-2.0/schemas/org.kylin.SettingsDaemon.plugins.media-keys.kylin-aiassistant.gschema.xml',
}
PREFIXES=('usr/share/voice-assistant/','usr/share/kylin-aiassistant/',
 'usr/share/kylin-user-guide/data/guide/kylin-aiassistant/','usr/local/share/ukui-air-gesture/')


def allowed(name):
    p=Path(name)
    return not p.is_absolute() and '..' not in p.parts and (name in EXACT or name.startswith(PREFIXES))


def install(stage,manifest):
    stage=stage.resolve();entries=json.loads(manifest.read_text())
    # Validate every entry before replacing any installed file.
    for item in entries:
        name=item['path'];source=stage/name
        if not allowed(name) or not source.resolve().is_relative_to(stage):raise RuntimeError('安装路径越界: '+name)
        if source.is_symlink() or not stat.S_ISREG(source.stat().st_mode):raise RuntimeError('安装源必须是普通文件: '+name)
        if hashlib.sha256(source.read_bytes()).hexdigest()!=item['sha256']:raise RuntimeError('构建产物校验失败: '+name)
        destination=Path('/')/name
        if destination.is_symlink() or destination.parent.resolve()!=destination.parent:
            # /usr-merged systems resolve /usr/lib normally; listed paths are
            # rooted in /usr or /etc and do not need alternate directory links.
            raise RuntimeError('安装目标包含未知符号链接: '+str(destination))
    for item in entries:
        source=stage/item['path'];destination=Path('/')/item['path']
        destination.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=destination.parent,prefix='.okrva-',delete=False) as f:
            temporary=Path(f.name);f.write(source.read_bytes());f.flush();os.fsync(f.fileno())
            os.fchmod(f.fileno(),item['mode'] & 0o777)
        os.replace(temporary,destination)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('stage',type=Path);parser.add_argument('manifest',type=Path)
    args=parser.parse_args()
    if os.geteuid()!=0:raise SystemExit('此安装步骤需要sudo')
    install(args.stage,args.manifest)
    print('系统程序文件安装完成')
