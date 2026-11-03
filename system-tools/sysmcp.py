#!/usr/bin/env python3
"""Bounded Bianbu desktop MCP tools; independent of the inference Adapter.

JSON-RPC is newline-delimited for KylinBot 0.7.4. No shell, arbitrary executable,
model-supplied environment, arbitrary file access, or privilege escalation.
Radio/brightness mutations require explicit server flags, off by default.
"""
from __future__ import annotations

from decimal import Decimal, ROUND_HALF_UP
import argparse
import contextlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import time

WPCTL = '/usr/bin/wpctl'
SINK = '@DEFAULT_AUDIO_SINK@'
MAX_LINE = 65536
VOLUME_PATTERN = re.compile(r'^Volume:\s+([0-9]+(?:\.[0-9]+)?)(?:\s+(\[MUTED\]))?\s*$')
SCHEMA = [
    {'name': 'set_volume', 'description': '设置系统播放音量百分比 (0-100)，执行后回读确认；不改变静音状态。',
     'inputSchema': {'type': 'object', 'properties': {
         'percent': {'type': 'integer', 'minimum': 0, 'maximum': 100,
                     'description': '目标音量百分比 0-100'}},
         'required': ['percent'], 'additionalProperties': False}},
    {'name': 'get_volume', 'description': '查询系统当前播放音量百分比及静音状态，不做修改。',
     'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False}},
]


def schema(name, description, properties=None, required=()):
    return {'name': name, 'description': description, 'inputSchema': {
        'type': 'object', 'properties': properties or {}, 'required': list(required),
        'additionalProperties': False}}


SCHEMA += [
    schema('adjust_volume', '按百分点增减当前音量，正数调大、负数调小；先查询再设置并回读，越界不执行。',
           {'delta': {'type': 'integer', 'minimum': -100, 'maximum': 100}}, ['delta']),
    schema('set_mute', '设置播放静音状态，true静音、false取消静音；不改变音量数值，执行后回读。',
           {'muted': {'type': 'boolean'}}, ['muted']),
    schema('wifi_status', '只读查询Wi-Fi无线开关与设备连接状态。'),
    schema('wifi_scan', '只读列出NetworkManager缓存的可见Wi-Fi，最多20项；不切换网络，不主动强制扫描。'),
    schema('set_wifi', '打开或关闭Wi-Fi射频开关；仅管理员事先显式启用该工具时提供，可能断网。',
           {'enabled': {'type': 'boolean'}}, ['enabled']),
    schema('bluetooth_status', '只读查询蓝牙软件和硬件射频阻断状态。'),
    schema('set_bluetooth', '设置蓝牙射频开关；仅事先显式启用时提供，不执行配对或连接。',
           {'enabled': {'type': 'boolean'}}, ['enabled']),
    schema('get_brightness', '只读查询本机背光亮度百分比和实际硬件读数。'),
    schema('set_brightness', '设置本机背光亮度百分比并回读；须事先启用且当前用户有写权限，不提权。',
           {'percent': {'type': 'integer', 'minimum': 1, 'maximum': 100}}, ['percent']),
    schema('open_application', '启动固定的桌面应用并检查窗口：file_manager文件管理器、system_settings系统设置。',
           {'application': {'type': 'string', 'enum': ['file_manager', 'system_settings']}}, ['application']),
    schema('get_capabilities', '查询当前可用系统工具及受限功能；工具缺失或操作不可用时先查询，不猜测支持情况。'),
]

COMMANDS = {'wpctl': '/usr/bin/wpctl', 'nmcli': '/usr/bin/nmcli',
            'rfkill': '/usr/sbin/rfkill', 'wlrctl': '/usr/bin/wlrctl'}
APPLICATIONS = {'file_manager': ('/usr/bin/pcmanfm-qt', 'pcmanfm-qt'),
                'system_settings': ('/usr/bin/lxqt-config', 'lxqt-config')}


class ToolError(Exception):
    def __init__(self, message: str, code: int = -32010):
        super().__init__(message)
        self.code = code


def desktop_environment():
    runtime = Path('/run/user') / str(os.getuid())
    info = runtime.lstat()
    if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid():
        raise ToolError('桌面运行目录无效，未执行操作。')
    result = {'PATH': '/usr/bin:/bin', 'LANG': 'C.UTF-8', 'LC_ALL': 'C.UTF-8',
              'XDG_RUNTIME_DIR': str(runtime), 'DBUS_SESSION_BUS_ADDRESS': 'unix:path=' + str(runtime / 'bus')}
    display = os.environ.get('DISPLAY', '')
    if re.fullmatch(r':[0-9]+(?:\.[0-9]+)?', display):
        result['DISPLAY'] = display
    wayland = os.environ.get('WAYLAND_DISPLAY', 'wayland-0')
    if re.fullmatch(r'[A-Za-z0-9_.-]+', wayland) and (runtime / wayland).is_socket():
        result['WAYLAND_DISPLAY'] = wayland
        result['QT_QPA_PLATFORM'] = 'wayland;xcb'
    authority = os.environ.get('XAUTHORITY', '')
    if authority and Path(authority).exists():
        info = Path(authority).lstat()
        if stat.S_ISREG(info.st_mode) and info.st_uid == os.getuid():
            result['XAUTHORITY'] = authority
    return result


def run_command(name, arguments):
    """Internal command IDs only; model input never selects an executable."""
    try:
        result = subprocess.run([COMMANDS[name], *arguments], shell=False,
                                capture_output=True, text=True, encoding='utf-8', errors='replace',
                                timeout=8, check=False, env=desktop_environment())
    except (OSError, subprocess.TimeoutExpired, KeyError) as error:
        raise ToolError('系统命令不可用或超时，未确认操作成功。') from error
    if result.returncode:
        raise ToolError(f'系统命令失败（退出码{result.returncode}）：' +
                        (result.stderr or result.stdout or 'no diagnostic').strip()[:400])
    return result.stdout.strip()


def application_environment():
    """GUI language/theme must not inherit the command parser's C locale."""
    import pwd
    result = dict(desktop_environment())
    result.pop('LC_ALL', None)
    result['HOME'] = pwd.getpwuid(os.getuid()).pw_dir
    for key in ['LANG', 'LC_MESSAGES', 'LANGUAGE']:
        value = os.environ.get(key, '')
        if re.fullmatch(r'[A-Za-z0-9_.@:\-]{1,128}', value):
            result[key] = value
    # Fixed desktop integration, not model-supplied plugin or search paths.
    result.update(QT_QPA_PLATFORMTHEME='lxqt', XDG_CURRENT_DESKTOP='LXQt')
    result['XDG_DATA_DIRS'] = ':'.join(path for path in
        ['/usr/share/bianbu-lite', '/usr/local/share', '/usr/share'] if Path(path).is_dir())
    result['XDG_CONFIG_DIRS'] = ':'.join(path for path in
        ['/etc/xdg/xdg-bianbu-lite', '/etc/xdg'] if Path(path).is_dir())
    return result


@contextlib.contextmanager
def volume_lock():
    """Serialize GUI and voice MCP processes sharing the same desktop volume."""
    import fcntl
    runtime = Path(desktop_environment()['XDG_RUNTIME_DIR'])
    descriptor = os.open(runtime / 'kylin-sys-tools-volume.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        info = os.fstat(descriptor)
        if info.st_uid != os.getuid() or not stat.S_ISREG(info.st_mode) or stat.S_IMODE(info.st_mode) != 0o600:
            raise ToolError('音量操作锁权限无效，未执行修改。')
        deadline = time.monotonic() + 3
        while True:
            try:
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                if time.monotonic() >= deadline:
                    raise ToolError('其他音量操作正在执行，请稍后重试。')
                time.sleep(.02)
        yield
    finally:
        os.close(descriptor)


def run_wpctl(arguments: list[str]) -> str:
    runtime = Path('/run/user') / str(os.getuid())
    try:
        info = runtime.lstat()
        if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid():
            raise ToolError('桌面音频运行目录无效，未执行音量操作。')
        # Do not inherit LD_PRELOAD, PYTHONPATH, shell startup or arbitrary PATH.
        environment = {'PATH': '/usr/bin:/bin', 'LANG': 'C.UTF-8',
                       'LC_ALL': 'C.UTF-8', 'XDG_RUNTIME_DIR': str(runtime)}
        result = subprocess.run([WPCTL, *arguments], shell=False, capture_output=True,
                                text=True, encoding='utf-8', errors='replace',
                                timeout=4, check=False, env=environment)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ToolError('音频命令不可用或超时；未确认操作成功，请重新查询实际音量。') from error
    if result.returncode != 0:
        detail = (result.stderr or result.stdout or 'no diagnostic').strip()[:400]
        raise ToolError(f'音频命令失败（退出码{result.returncode}），未确认操作成功：{detail}')
    return result.stdout.strip()


def parse_volume(text: str) -> tuple[int, bool]:
    match = VOLUME_PATTERN.fullmatch(text)
    if not match:
        raise ToolError('无法识别实际音量读数，未确认操作成功。')
    percent = int((Decimal(match.group(1)) * 100).quantize(Decimal('1'), rounding=ROUND_HALF_UP))
    return percent, bool(match.group(2))


class VolumeTools:
    def __init__(self, runner=run_wpctl):
        self.runner = runner

    def readback(self, matches):
        """Poll state, never repeat a setter or a relative adjustment.

        PipeWire updates can be asynchronous. Stop after four reads, and do not
        start another read after the one-second observation window. Each command
        retains its existing timeout; command/parse failures still fail at once.
        """
        deadline = time.monotonic() + 1
        for delay in (0, .05, .1, .15):
            if delay:
                if time.monotonic() + delay >= deadline:
                    break
                time.sleep(delay)
            actual = parse_volume(self.runner(['get-volume', SINK]))
            if matches(*actual):
                return actual
        return actual

    def call(self, name: str, arguments: object) -> str:
        if not isinstance(arguments, dict):
            raise ToolError('工具参数必须是JSON对象。', -32602)
        if name == 'get_volume':
            if arguments:
                raise ToolError('get_volume不接受参数。', -32602)
            percent, muted = parse_volume(self.runner(['get-volume', SINK]))
            return f'系统当前音量为{percent}%，' + ('已静音。' if muted else '未静音。')
        if name != 'set_volume':
            raise ToolError('未开放此工具。', -32602)
        if set(arguments) != {'percent'}:
            raise ToolError('set_volume仅接受percent参数。', -32602)
        percent = arguments['percent']
        if type(percent) is not int or not 0 <= percent <= 100:
            raise ToolError('percent必须是0到100的整数；未执行设置。', -32602)
        # Never combine setter and getter with ';': each exit status matters.
        self.runner(['set-volume', SINK, f'{percent}%'])
        actual, muted = self.readback(lambda actual, muted: actual == percent)
        if actual != percent:
            raise ToolError(f'音量验证失败：目标{percent}%，实际{actual}%；不能报告设置成功。')
        return (f'系统音量已设置为{actual}%，已回读确认。'
                + ('当前仍处于静音状态。' if muted else ''))


class Backlight:
    def __init__(self, root=Path('/sys/class/backlight')):
        self.root = Path(root)

    def device(self):
        devices = sorted(self.root.iterdir()) if self.root.exists() else []
        if len(devices) != 1:
            raise ToolError('未找到唯一可用的背光设备；未执行修改。')
        device = devices[0].resolve(strict=True)
        if self.root == Path('/sys/class/backlight') and not device.is_relative_to('/sys/devices'):
            raise ToolError('背光设备路径不可信。')
        return device

    def read(self):
        try:
            device = self.device()
            maximum = int((device / 'max_brightness').read_text().strip())
            actual = int((device / 'actual_brightness').read_text().strip())
            if not maximum > 0 or not 0 <= actual <= maximum:
                raise ValueError('invalid hardware value')
            return device, maximum, actual
        except (OSError, ValueError) as error:
            raise ToolError('无法读取有效背光状态，未确认操作成功。') from error

    def writable(self):
        try:
            return os.access(self.device() / 'brightness', os.W_OK)
        except (OSError, ToolError):
            return False

    def set(self, percent):
        device, maximum, _ = self.read()
        target = (maximum * percent + 50) // 100
        if target <= 0:
            raise ToolError('目标亮度会使屏幕熄灭，未执行修改。')
        try:
            descriptor = os.open(device / 'brightness', os.O_WRONLY | os.O_NOFOLLOW)
            with os.fdopen(descriptor, 'w') as stream:
                stream.write(str(target) + '\n')
        except OSError as error:
            raise ToolError('当前用户不能修改亮度；没有提权，未确认成功。') from error
        for _ in range(10):
            _, _, actual = self.read()
            if actual == target:
                return f'背光亮度已设置为{percent}%，实际硬件值{actual}/{maximum}，已回读确认。'
            time.sleep(.05)
        raise ToolError(f'亮度验证失败：目标硬件值{target}，实际{actual}；不能报告成功。')


def split_nmcli(line):
    fields, current, escaped = [], '', False
    for char in line:
        if escaped:
            current += char
            escaped = False
        elif char == '\\':
            escaped = True
        elif char == ':':
            fields.append(current)
            current = ''
        else:
            current += char
    fields.append(current + ('\\' if escaped else ''))
    return fields


class SystemTools(VolumeTools):
    def __init__(self, commands=run_command, available=None, backlight=None,
                 allow_radio=False, allow_brightness=False, lock=volume_lock, launcher=None):
        self.commands = commands
        self.available = (lambda key: os.access(COMMANDS[key], os.X_OK)) if available is None else available
        self.backlight = backlight or Backlight()
        self.allow_radio = allow_radio
        self.allow_brightness = allow_brightness
        self.lock = lock
        self.launcher = launcher or self.launch_application
        self.children = []
        super().__init__(lambda arguments: commands('wpctl', arguments))

    def capabilities(self):
        audio, network, bluetooth = [self.available(key) for key in ['wpctl', 'nmcli', 'rfkill']]
        try:
            self.backlight.read()
            brightness = True
        except ToolError:
            brightness = False
        apps = [name for name, (binary, _) in APPLICATIONS.items() if os.access(binary, os.X_OK)]
        enabled = ['get_capabilities']
        if audio:
            enabled += ['set_volume', 'get_volume', 'adjust_volume', 'set_mute']
        if network:
            enabled += ['wifi_status', 'wifi_scan']
            if self.allow_radio:
                enabled += ['set_wifi']
        if bluetooth:
            enabled += ['bluetooth_status']
            if self.allow_radio:
                enabled += ['set_bluetooth']
        if brightness:
            enabled += ['get_brightness']
            if self.allow_brightness and self.backlight.writable():
                enabled += ['set_brightness']
        if apps:
            enabled += ['open_application']
        return {'enabled_tools': enabled, 'applications': apps,
                'radio_changes_enabled': self.allow_radio,
                'brightness_changes_enabled': self.allow_brightness and brightness and self.backlight.writable(),
                'privilege_escalation': False}

    def schemas(self):
        enabled = set(self.capabilities()['enabled_tools'])
        return [item for item in SCHEMA if item['name'] in enabled]

    def checked(self, name, arguments):
        if not isinstance(arguments, dict):
            raise ToolError('工具参数必须是JSON对象。', -32602)
        item = next((item for item in SCHEMA if item['name'] == name), None)
        if item is None:
            raise ToolError('未开放此工具。', -32602)
        spec = item['inputSchema']
        required = set(spec.get('required', []))
        if set(arguments) != required:
            raise ToolError('工具参数缺失或包含未知字段；未执行操作。', -32602)
        for key, value in arguments.items():
            rule = spec['properties'][key]
            valid = {'integer': int, 'boolean': bool, 'string': str}[rule['type']]
            if type(value) is not valid:
                raise ToolError('工具参数类型错误；未执行操作。', -32602)
            if ('minimum' in rule and value < rule['minimum'] or
                    'maximum' in rule and value > rule['maximum'] or
                    'enum' in rule and value not in rule['enum']):
                raise ToolError('工具参数超出允许范围；未执行操作。', -32602)
        if name not in self.capabilities()['enabled_tools']:
            raise ToolError('此操作当前未启用或设备不支持；未执行修改。请查询get_capabilities。')

    def call(self, name, arguments):
        self.checked(name, arguments)
        if name in ['get_volume', 'set_volume', 'adjust_volume', 'set_mute']:
            with self.lock():
                if name in ['get_volume', 'set_volume']:
                    return super().call(name, arguments)
                actual, muted = parse_volume(self.runner(['get-volume', SINK]))
                if name == 'adjust_volume':
                    target = actual + arguments['delta']
                    if not 0 <= target <= 100:
                        raise ToolError(f'当前音量{actual}%，增减{arguments["delta"]}个百分点将越界；未执行修改。')
                    return super().call('set_volume', {'percent': target})
                self.runner(['set-mute', SINK, '1' if arguments['muted'] else '0'])
                after, muted_after = self.readback(lambda after, muted_after: muted_after == arguments['muted'] and after == actual)
                if muted_after != arguments['muted'] or after != actual:
                    raise ToolError('静音状态回读与目标不符，或音量数值发生变化；未确认操作成功。')
                return f'系统已{("静音" if muted_after else "取消静音")}，音量仍为{after}%，已回读确认。'
        if name == 'get_capabilities':
            return json.dumps(self.capabilities(), ensure_ascii=False)
        if name == 'wifi_status':
            radio = self.wifi_radio()
            devices = self.commands('nmcli', ['-t', '--escape', 'yes', '-f', 'DEVICE,TYPE,STATE,CONNECTION', 'device', 'status'])
            return json.dumps({'wifi_enabled': radio, 'devices': [split_nmcli(line) for line in devices.splitlines()
                               if len(split_nmcli(line)) >= 2 and split_nmcli(line)[1] == 'wifi']}, ensure_ascii=False)
        if name == 'wifi_scan':
            rows = self.commands('nmcli', ['-t', '--escape', 'yes', '-f', 'SSID,SIGNAL,SECURITY', 'device', 'wifi', 'list', '--rescan', 'no'])
            return json.dumps({'source': 'NetworkManager cache', 'networks': [split_nmcli(line) for line in rows.splitlines()[:20]]}, ensure_ascii=False)
        if name == 'set_wifi':
            self.commands('nmcli', ['radio', 'wifi', 'on' if arguments['enabled'] else 'off'])
            if self.wifi_radio() != arguments['enabled']:
                raise ToolError('Wi-Fi射频开关回读不符；未确认成功。')
            return 'Wi-Fi射频开关已' + ('打开' if arguments['enabled'] else '关闭') + '，已回读；不代表已经连接网络。'
        if name == 'bluetooth_status':
            return json.dumps({'bluetooth': self.bluetooth()}, ensure_ascii=False)
        if name == 'set_bluetooth':
            self.commands('rfkill', ['unblock' if arguments['enabled'] else 'block', 'bluetooth'])
            radios = self.bluetooth()
            if not radios or any((item['soft'] == 'unblocked' and item['hard'] == 'unblocked') != arguments['enabled'] for item in radios):
                raise ToolError('蓝牙射频状态回读不符；未确认成功。')
            return '蓝牙射频已' + ('打开' if arguments['enabled'] else '关闭') + '，已回读；未进行配对或连接。'
        if name == 'get_brightness':
            _, maximum, actual = self.backlight.read()
            return f'当前背光亮度约{(100 * actual + maximum // 2) // maximum}%，硬件实际值{actual}/{maximum}。'
        if name == 'set_brightness':
            return self.backlight.set(arguments['percent'])
        if name == 'open_application':
            if arguments['application'] not in self.capabilities()['applications']:
                raise ToolError('所选桌面应用未安装；未启动。')
            return self.launcher(arguments['application'])
        raise ToolError('未实现的工具。', -32601)

    def wifi_radio(self):
        value = self.commands('nmcli', ['-t', '-f', 'WIFI', 'radio'])
        if value not in ['enabled', 'disabled']:
            raise ToolError('无法识别Wi-Fi射频状态；未确认成功。')
        return value == 'enabled'

    def bluetooth(self):
        try:
            rows = json.loads(self.commands('rfkill', ['--json', '--output', 'ID,TYPE,SOFT,HARD']))['rfkilldevices']
            radios = [row for row in rows if row['type'] == 'bluetooth']
            if any(row['soft'] not in ['blocked', 'unblocked'] or row['hard'] not in ['blocked', 'unblocked'] for row in radios):
                raise ValueError('unexpected block state')
            return radios
        except (ValueError, TypeError, KeyError) as error:
            raise ToolError('无法读取有效蓝牙状态。') from error

    def launch_application(self, application):
        binary, app_id = APPLICATIONS[application]
        self.children = [child for child in self.children if child.poll() is None]
        try:
            child = subprocess.Popen([binary], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.DEVNULL, shell=False, start_new_session=True,
                                     env=application_environment())
            self.children.append(child)
        except OSError as error:
            raise ToolError('桌面应用启动失败。') from error
        # Qt applications can take several seconds on a cold K1 while the LLM
        # is active. Stay below the configured 25-second MCP deadline.
        deadline = time.monotonic() + 12
        while time.monotonic() < deadline:
            if child.poll() not in [None, 0]:
                raise ToolError('应用启动进程异常退出，未确认窗口已打开。')
            if self.available('wlrctl'):
                try:
                    self.commands('wlrctl', ['toplevel', 'find', 'app_id:' + app_id])
                    return ('文件管理器' if application == 'file_manager' else '系统设置') + '窗口已检测到。'
                except ToolError:
                    pass
            time.sleep(.1)
        raise ToolError('启动请求已发送，但未能确认应用窗口出现；不能报告已打开。')


def rpc_error(request_id, code: int, message: str):
    return {'jsonrpc': '2.0', 'id': request_id, 'error': {'code': code, 'message': message}}


def handle(message: object, tools: VolumeTools):
    if not isinstance(message, dict) or message.get('jsonrpc') != '2.0':
        return rpc_error(None, -32600, 'Invalid JSON-RPC request')
    request_id = message.get('id')
    method = message.get('method')
    if not isinstance(method, str):
        return rpc_error(request_id, -32600, 'Invalid JSON-RPC method')
    # Notifications, including a notification named tools/call, never mutate.
    if request_id is None:
        return None
    if type(request_id) not in (str, int):
        return rpc_error(None, -32600, 'Invalid JSON-RPC id')
    params = message.get('params', {})
    if not isinstance(params, dict):
        return rpc_error(request_id, -32602, 'Parameters must be an object')
    try:
        if method == 'initialize':
            result = {'protocolVersion': '2024-11-05',
                      'capabilities': {'tools': {'listChanged': False}},
                      'serverInfo': {'name': 'kylin-sys-tools', 'version': '0.2.0'}}
        elif method == 'ping':
            result = {}
        elif method == 'tools/list':
            result = {'tools': tools.schemas() if hasattr(tools, 'schemas') else SCHEMA[:2]}
        elif method == 'tools/call':
            if set(params) - {'name', 'arguments'}:
                raise ToolError('未知调用字段。', -32602)
            text = tools.call(params.get('name'), params.get('arguments', {}))
            result = {'content': [{'type': 'text', 'text': text}], 'isError': False}
        else:
            return rpc_error(request_id, -32601, 'Method not found')
    except ToolError as error:
        # KylinBot 0.7.4 ignores result.isError, but propagates JSON-RPC errors.
        # Explicit application errors prevent failed actions being marked success.
        return rpc_error(request_id, error.code, str(error))
    return {'jsonrpc': '2.0', 'id': request_id, 'result': result}


def serve(source, target, tools=None):
    tools = tools or SystemTools()
    while True:
        line = source.readline(MAX_LINE + 1)
        if not line:
            return
        if len(line) > MAX_LINE:
            # Bound input memory; discard the rest of this oversized request.
            while line and not line.endswith('\n'):
                line = source.readline(MAX_LINE + 1)
            response = rpc_error(None, -32600, 'Request exceeds input limit')
        elif not line.strip():
            continue
        else:
            try:
                response = handle(json.loads(line), tools)
            except json.JSONDecodeError:
                response = rpc_error(None, -32700, 'Invalid JSON')
            except Exception:
                response = rpc_error(None, -32603, 'Internal error; action success not confirmed')
        if response is not None:
            target.write(json.dumps(response, ensure_ascii=False) + '\n')
            target.flush()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--allow-radio-changes', action='store_true', help='explicitly permit Wi-Fi/Bluetooth radio mutations')
    parser.add_argument('--allow-brightness-changes', action='store_true', help='permit brightness writes only with existing user permissions')
    parser.add_argument('--check', action='store_true', help='print read-only capability discovery and exit')
    options = parser.parse_args()
    tools = SystemTools(allow_radio=options.allow_radio_changes,
                        allow_brightness=options.allow_brightness_changes)
    if options.check:
        print(json.dumps(tools.capabilities(), ensure_ascii=False, indent=2))
    else:
        serve(sys.stdin, sys.stdout, tools)
