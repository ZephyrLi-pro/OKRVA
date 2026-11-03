import contextlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('sysmcp', Path(__file__).with_name('sysmcp.py'))
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


class Backlight:
    def read(self):
        return Path('/fake/backlight'), 255, 128

    def writable(self):
        return False

    def set(self, percent):
        raise AssertionError('must not write real brightness')


class Commands:
    def __init__(self, answers=()):
        self.answers = iter(answers)
        self.calls = []

    def __call__(self, name, arguments):
        self.calls.append((name, arguments))
        value = next(self.answers)
        if isinstance(value, Exception):
            raise value
        return value


class SystemTests(unittest.TestCase):
    def setUp(self):
        # Installed application discovery is independent of the test host OS.
        self.apps = patch.dict(server.APPLICATIONS, {'file_manager': (sys.executable, 'pcmanfm-qt'),
                                                    'system_settings': (sys.executable, 'lxqt-config')}, clear=True)
        self.apps.start()
        self.addCleanup(self.apps.stop)

    def tools(self, answers=(), **kwargs):
        commands = Commands(answers)
        return server.SystemTools(commands=commands, available=lambda _: True, backlight=Backlight(),
                                  lock=contextlib.nullcontext, **kwargs), commands

    def test_catalog_default_does_not_offer_radio_or_brightness_mutation(self):
        tools, commands = self.tools()
        names = {item['name'] for item in tools.schemas()}
        self.assertFalse(names & {'set_wifi', 'set_bluetooth', 'set_brightness'})
        self.assertTrue({'adjust_volume', 'set_mute', 'open_application', 'get_volume', 'get_brightness'} <= names)
        self.assertEqual(commands.calls, [])

    def test_missing_executables_are_not_advertised(self):
        tools, _ = self.tools()
        tools.available = lambda _: False
        names = {item['name'] for item in tools.schemas()}
        self.assertFalse(names & {'get_volume', 'set_volume', 'wifi_scan', 'bluetooth_status'})

    def test_radio_blocked_even_if_manually_called(self):
        tools, commands = self.tools()
        for name in ['set_wifi', 'set_bluetooth']:
            with self.assertRaisesRegex(server.ToolError, '未启用'):
                tools.call(name, {'enabled': False})
        self.assertEqual(commands.calls, [])

    def test_adjust_volume_reads_then_sets_and_reads_back(self):
        tools, commands = self.tools(['Volume: 0.35', '', 'Volume: 0.45'])
        self.assertIn('45%', tools.call('adjust_volume', {'delta': 10}))
        self.assertEqual(commands.calls, [('wpctl', ['get-volume', server.SINK]),
            ('wpctl', ['set-volume', server.SINK, '45%']), ('wpctl', ['get-volume', server.SINK])])

    def test_adjust_underflow_overflow_fail_without_setting(self):
        for volume, delta in [('Volume: 0.95', 10), ('Volume: 0.05', -10)]:
            tools, commands = self.tools([volume])
            with self.assertRaisesRegex(server.ToolError, '越界'):
                tools.call('adjust_volume', {'delta': delta})
            self.assertEqual(len(commands.calls), 1)

    def test_adjust_preserves_mute(self):
        tools, _ = self.tools(['Volume: 0.35 [MUTED]', '', 'Volume: 0.25 [MUTED]'])
        self.assertIn('仍处于静音', tools.call('adjust_volume', {'delta': -10}))

    def test_delayed_relative_readback_never_repeats_increment(self):
        tools, commands = self.tools(['Volume: 0.35', '', 'Volume: 0.35', 'Volume: 0.45'])
        with patch.object(server.time, 'sleep'):
            self.assertIn('45%', tools.call('adjust_volume', {'delta': 10}))
        self.assertEqual(sum(args[0] == 'set-volume' for _, args in commands.calls), 1)

    def test_delayed_mute_readback_never_repeats_setter(self):
        tools, commands = self.tools(['Volume: 0.35', '', 'Volume: 0.35', 'Volume: 0.35 [MUTED]'])
        with patch.object(server.time, 'sleep'):
            self.assertIn('已回读', tools.call('set_mute', {'muted': True}))
        self.assertEqual(sum(args[0] == 'set-mute' for _, args in commands.calls), 1)

    def test_mute_strict_boolean_and_readback(self):
        for muted, after in [(True, 'Volume: 0.35 [MUTED]'), (False, 'Volume: 0.35')]:
            tools, commands = self.tools(['Volume: 0.35', '', after])
            self.assertIn('已回读', tools.call('set_mute', {'muted': muted}))
            self.assertEqual(commands.calls[1], ('wpctl', ['set-mute', server.SINK, '1' if muted else '0']))

    def test_invalid_mutation_values_never_execute(self):
        for name, argument, values in [('set_mute', 'muted', ['false', 0, 1, None]),
                ('adjust_volume', 'delta', [True, '10', 10.0, -101, 101]),
                ('set_wifi', 'enabled', [1, 'true']), ('set_brightness', 'percent', [True, 0, 101]),
                ('open_application', 'application', ['shell', 'file_manager;touch /tmp/pwn', True])]:
            for value in values:
                with self.subTest(name=name, value=value):
                    tools, commands = self.tools(allow_radio=True)
                    with self.assertRaises(server.ToolError):
                        tools.call(name, {argument: value})
                    self.assertEqual(commands.calls, [])

    def test_extra_missing_or_nonobject_parameters_rejected(self):
        tools, commands = self.tools()
        for args in [None, [], {}, {'muted': False, 'command': 'something'}]:
            with self.assertRaises(server.ToolError):
                tools.call('set_mute', args)
        self.assertEqual(commands.calls, [])

    def test_mute_mismatch_and_volume_change_are_failures(self):
        for after in ['Volume: 0.35', 'Volume: 0.40 [MUTED]']:
            tools, _ = self.tools(['Volume: 0.35', ''] + [after] * 4)
            with self.assertRaisesRegex(server.ToolError, '回读'):
                tools.call('set_mute', {'muted': True})

    def test_mutation_failed_setter_not_hidden_by_readback(self):
        tools, commands = self.tools(['Volume: 0.35', server.ToolError('setter failed')])
        with self.assertRaisesRegex(server.ToolError, 'setter failed'):
            tools.call('set_mute', {'muted': True})
        self.assertEqual(len(commands.calls), 2)

    def test_wifi_status_does_not_hardcode_device_name(self):
        tools, commands = self.tools(['enabled', 'wlp7s0:wifi:connected:Office\\:LAN\neth0:ethernet:connected:wire'])
        result = json.loads(tools.call('wifi_status', {}))
        self.assertTrue(result['wifi_enabled'])
        self.assertEqual(result['devices'], [['wlp7s0', 'wifi', 'connected', 'Office:LAN']])
        self.assertFalse(any('wlan0' in str(call) for call in commands.calls))

    def test_wifi_scan_is_cached_bounded_and_has_no_connect_command(self):
        tools, commands = self.tools(['\n'.join(f'AP{i}:50:WPA2' for i in range(30))])
        self.assertEqual(len(json.loads(tools.call('wifi_scan', {}))['networks']), 20)
        self.assertEqual(commands.calls[0][1][-2:], ['--rescan', 'no'])

    def test_wifi_set_requires_explicit_enable_and_matching_state(self):
        tools, commands = self.tools(['', 'disabled'], allow_radio=True)
        self.assertIn('关闭', tools.call('set_wifi', {'enabled': False}))
        self.assertEqual(commands.calls[0], ('nmcli', ['radio', 'wifi', 'off']))
        tools, _ = self.tools(['', 'enabled'], allow_radio=True)
        with self.assertRaisesRegex(server.ToolError, '回读'):
            tools.call('set_wifi', {'enabled': False})

    def test_wifi_unknown_state_is_error(self):
        tools, _ = self.tools(['maybe'])
        with self.assertRaises(server.ToolError):
            tools.call('wifi_status', {})

    def test_bluetooth_status_reads_json_and_preserves_hard_block(self):
        tools, _ = self.tools([json.dumps({'rfkilldevices': [{'id': 2, 'type': 'bluetooth',
                              'soft': 'unblocked', 'hard': 'blocked'}]})])
        result = json.loads(tools.call('bluetooth_status', {}))
        self.assertEqual(result['bluetooth'][0]['hard'], 'blocked')

    def test_bluetooth_hardware_block_is_not_successful_power_on(self):
        tools, _ = self.tools(['', json.dumps({'rfkilldevices': [{'id': 2, 'type': 'bluetooth',
                              'soft': 'unblocked', 'hard': 'blocked'}]})], allow_radio=True)
        with self.assertRaisesRegex(server.ToolError, '回读'):
            tools.call('set_bluetooth', {'enabled': True})

    def test_bluetooth_bad_json_and_bad_states_are_errors(self):
        for response in ['not-json', '{}', '{"rfkilldevices":null}',
                         '{"rfkilldevices":[{"type":"bluetooth","soft":"unknown","hard":"blocked"}]}']:
            tools, _ = self.tools([response])
            with self.assertRaises(server.ToolError):
                tools.call('bluetooth_status', {})

    def test_brightness_default_no_write_and_status_only(self):
        tools, _ = self.tools()
        self.assertIn('128/255', tools.call('get_brightness', {}))
        with self.assertRaisesRegex(server.ToolError, '未启用'):
            tools.call('set_brightness', {'percent': 50})

    def test_brightness_flag_cannot_bypass_os_permissions(self):
        tools, _ = self.tools(allow_brightness=True)
        self.assertNotIn('set_brightness', tools.capabilities()['enabled_tools'])

    def test_launcher_is_enum_only(self):
        launched = []
        tools, _ = self.tools(launcher=lambda name: launched.append(name) or '已检测到窗口')
        self.assertIn('窗口', tools.call('open_application', {'application': 'file_manager'}))
        self.assertEqual(launched, ['file_manager'])
        with self.assertRaises(server.ToolError):
            tools.call('open_application', {'application': '/bin/sh'})

    def test_cold_app_window_can_be_confirmed_after_three_seconds(self):
        tools, commands = self.tools([server.ToolError('no window yet'), 'window found'])
        with patch.object(server, 'application_environment', return_value={}), \
                patch.object(server.subprocess, 'Popen') as popen, \
                patch.object(server.time, 'monotonic', side_effect=[0, 1, 5]), \
                patch.object(server.time, 'sleep'):
            popen.return_value.poll.return_value = None
            self.assertIn('窗口已检测到', tools.launch_application('system_settings'))
        self.assertEqual(commands.calls, [('wlrctl', ['toplevel', 'find', 'app_id:lxqt-config'])] * 2)
        self.assertFalse(popen.call_args.kwargs['shell'])

    def test_gui_keeps_desktop_language_and_theme_but_not_loader_environment(self):
        passwd = SimpleNamespace(getpwuid=lambda _: SimpleNamespace(pw_dir='/home/test'))
        parser = {'LANG': 'C.UTF-8', 'LC_ALL': 'C.UTF-8', 'WAYLAND_DISPLAY': 'wayland-0'}
        with patch.object(server, 'desktop_environment', return_value=parser), \
                patch.dict(sys.modules, {'pwd': passwd}), \
                patch.object(server.os, 'getuid', return_value=1000, create=True), \
                patch.dict(server.os.environ, {'LANG': 'zh_CN.UTF-8', 'LANGUAGE': 'zh_CN:zh',
                    'LD_PRELOAD': '/untrusted/lib.so', 'QT_PLUGIN_PATH': '/untrusted'}, clear=True):
            result = server.application_environment()
        self.assertEqual(result['LANG'], 'zh_CN.UTF-8')
        self.assertEqual(result['QT_QPA_PLATFORMTHEME'], 'lxqt')
        self.assertEqual(result['HOME'], '/home/test')
        self.assertNotIn('LC_ALL', result)
        self.assertNotIn('LD_PRELOAD', result)
        self.assertNotIn('QT_PLUGIN_PATH', result)
        self.assertEqual(parser['LC_ALL'], 'C.UTF-8', 'command parser environment was mutated')

    def test_failed_execution_is_jsonrpc_error_not_ignored_iserror(self):
        tools, _ = self.tools([server.ToolError('real command failed')])
        result = server.handle({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
                                'params': {'name': 'wifi_status', 'arguments': {}}}, tools)
        self.assertEqual(result['error']['code'], -32010)
        self.assertNotIn('result', result)

    def test_tools_list_matches_actual_policy(self):
        tools, _ = self.tools()
        result = server.handle({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/list'}, tools)
        self.assertEqual(result['result']['tools'], tools.schemas())

    def test_command_dispatch_uses_fixed_argv_no_shell(self):
        completed = subprocess.CompletedProcess([], 0, 'enabled\n', '')
        with patch.object(server, 'desktop_environment', return_value={'LANG': 'C.UTF-8'}), \
                patch.object(server.subprocess, 'run', return_value=completed) as run:
            self.assertEqual(server.run_command('nmcli', ['-t', '-f', 'WIFI', 'radio']), 'enabled')
        self.assertEqual(run.call_args.args[0], ['/usr/bin/nmcli', '-t', '-f', 'WIFI', 'radio'])
        self.assertFalse(run.call_args.kwargs['shell'])
        with self.assertRaises(server.ToolError), patch.object(server.subprocess, 'run') as run:
            server.run_command('/bin/sh', ['-c', 'anything'])
        run.assert_not_called()

    def test_bad_command_status_and_timeout_are_errors(self):
        with patch.object(server, 'desktop_environment', return_value={}), \
                patch.object(server.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1, '', 'failed')):
            with self.assertRaises(server.ToolError):
                server.run_command('nmcli', ['radio'])


class BacklightTests(unittest.TestCase):
    def test_valid_read_and_invalid_range(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            device = root / 'panel'
            device.mkdir()
            (device / 'max_brightness').write_text('255')
            (device / 'actual_brightness').write_text('128')
            self.assertEqual(server.Backlight(root).read()[1:], (255, 128))
            (device / 'actual_brightness').write_text('999')
            with self.assertRaises(server.ToolError):
                server.Backlight(root).read()

    def test_multiple_or_missing_devices_not_guessed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            backlight = server.Backlight(root)
            with self.assertRaises(server.ToolError):
                backlight.read()
            (root / 'a').mkdir()
            (root / 'b').mkdir()
            with self.assertRaises(server.ToolError):
                backlight.read()


if __name__ == '__main__':
    unittest.main()
