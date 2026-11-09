"""Read-only capability checks for the selected native Impress runtime."""
from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess

from common import InstallError

DEFAULT_IMPRESS_PIPE = 'rvai_trial_248_1000'


def runtime_settings(home=None, launcher=None, pipe=None):
    home = Path.home() if home is None else Path(home)
    launcher = launcher or os.environ.get('RVAI_LIBREOFFICE_LAUNCHER') or home/'.local/bin/libreoffice-24.8'
    launcher = Path(launcher).expanduser()
    if not launcher.is_absolute():
        raise InstallError('RVAI_LIBREOFFICE_LAUNCHER必须为绝对路径。')
    pipe = pipe or os.environ.get('RVAI_IMPRESS_PIPE') or DEFAULT_IMPRESS_PIPE
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,128}', pipe):
        raise InstallError('RVAI_IMPRESS_PIPE只能包含字母、数字、下划线或连字符。')
    return dict(launcher=str(launcher), pipe=pipe)


def check_libreoffice(home=None, launcher=None, pipe=None):
    """Check version without opening a PPT or changing the user's profile."""
    settings = runtime_settings(home, launcher, pipe)
    executable = Path(settings['launcher'])
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise InstallError('缺少可执行的LibreOffice 24.8启动器: '+str(executable)+
            '\n请先准备已验证的24.8运行环境；安装器不会把系统24.2视为已迁移，也不会自动下载或替换LibreOffice。')
    try:
        result = subprocess.run([str(executable), '--version'], text=True,
                                capture_output=True, timeout=30)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise InstallError('LibreOffice 24.8启动器版本检查失败: '+type(error).__name__) from error
    match = re.search(r'(?m)^LibreOffice\s+(\d+)\.(\d+)(?:\.(\d+))?', result.stdout)
    if result.returncode or not match:
        raise InstallError('LibreOffice启动器未返回可识别的版本，请检查独立运行库: '+str(executable))
    version = tuple(int(value) for value in match.groups(default='0'))
    if version[:2] < (24, 8):
        raise InstallError('底部备注栏需要LibreOffice 24.8或以上，检测到 '+match.group(0)+
                           '；尚未完成24.8运行入口迁移。')
    return dict(settings, version='.'.join(map(str, version)), version_check_passed=True)
