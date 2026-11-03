import importlib.util
import io
import json
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('volume_server', Path(__file__).with_name('server.py'))
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


class FakeRunner:
    def __init__(self, answers):
        self.answers = iter(answers)
        self.calls = []

    def __call__(self, arguments):
        self.calls.append(arguments)
        answer = next(self.answers)
        if isinstance(answer, Exception):
            raise answer
        return answer


class VolumeTests(unittest.TestCase):
    def test_valid_setting_and_separate_readback(self):
        runner = FakeRunner(['', 'Volume: 0.50'])
        result = server.VolumeTools(runner).call('set_volume', {'percent': 50})
        self.assertIn('已设置为50%', result)
        self.assertEqual(runner.calls, [['set-volume', server.SINK, '50%'], ['get-volume', server.SINK]])

    def test_boundaries_are_valid(self):
        for percent, text in [(0, 'Volume: 0.00'), (100, 'Volume: 1.00')]:
            with self.subTest(percent=percent):
                runner = FakeRunner(['', text])
                self.assertIn(f'{percent}%', server.VolumeTools(runner).call('set_volume', {'percent': percent}))

    def test_invalid_values_never_execute(self):
        for value in [True, False, -1, 101, 50.0, '50', '50%; touch /tmp/pwn', None, [], {}]:
            with self.subTest(value=value):
                runner = FakeRunner([])
                with self.assertRaises(server.ToolError):
                    server.VolumeTools(runner).call('set_volume', {'percent': value})
                self.assertEqual(runner.calls, [])

    def test_unknown_arguments_never_execute(self):
        for args in [{}, {'percent': 50, 'command': 'echo unsafe'}, [], None]:
            runner = FakeRunner([])
            with self.assertRaises(server.ToolError):
                server.VolumeTools(runner).call('set_volume', args)
            self.assertEqual(runner.calls, [])

    def test_write_failure_is_not_hidden_by_getter(self):
        runner = FakeRunner([server.ToolError('setter failed')])
        with self.assertRaisesRegex(server.ToolError, 'setter failed'):
            server.VolumeTools(runner).call('set_volume', {'percent': 50})
        self.assertEqual(len(runner.calls), 1)

    def test_readback_failure_does_not_claim_success(self):
        runner = FakeRunner(['', server.ToolError('readback failed')])
        with self.assertRaisesRegex(server.ToolError, 'readback failed'):
            server.VolumeTools(runner).call('set_volume', {'percent': 50})

    def test_mismatch_is_failure(self):
        runner = FakeRunner(['', 'Volume: 0.67'])
        with self.assertRaisesRegex(server.ToolError, '验证失败'):
            server.VolumeTools(runner).call('set_volume', {'percent': 50})

    def test_mute_is_preserved_and_reported(self):
        runner = FakeRunner(['', 'Volume: 0.50 [MUTED]'])
        self.assertIn('仍处于静音', server.VolumeTools(runner).call('set_volume', {'percent': 50}))
        self.assertNotIn('set-mute', str(runner.calls))

    def test_get_volume_is_read_only(self):
        runner = FakeRunner(['Volume: 0.67'])
        self.assertIn('当前音量为67%', server.VolumeTools(runner).call('get_volume', {}))
        self.assertEqual(runner.calls, [['get-volume', server.SINK]])

    def test_getter_rejects_parameters(self):
        with self.assertRaises(server.ToolError):
            server.VolumeTools(FakeRunner([])).call('get_volume', {'percent': 50})

    def test_strict_readback_parser(self):
        for text in ['Volume: NaN', 'Volume: inf', '50%', 'Volume: 0.50 garbage', '', 'Volume: -1']:
            with self.assertRaises(server.ToolError):
                server.parse_volume(text)

    def test_unknown_tool_is_rejected(self):
        for name in ['shell', 'set_wifi', 'set_bluetooth', None]:
            with self.assertRaises(server.ToolError):
                server.VolumeTools(FakeRunner([])).call(name, {})

    def test_subprocess_fixed_argv_and_clean_environment(self):
        info = type('Stat', (), {'st_mode': 0o40700, 'st_uid': 1000})()
        completed = subprocess.CompletedProcess([], 0, stdout='Volume: 0.50\n', stderr='')
        with patch.object(server.os, 'getuid', return_value=1000, create=True), \
                patch.object(server.Path, 'lstat', return_value=info), \
                patch.object(server.subprocess, 'run', return_value=completed) as run:
            self.assertEqual(server.run_wpctl(['get-volume', server.SINK]), 'Volume: 0.50')
        args, kwargs = run.call_args
        self.assertEqual(args[0], ['/usr/bin/wpctl', 'get-volume', server.SINK])
        self.assertFalse(kwargs['shell'])
        self.assertEqual(kwargs['timeout'], 4)
        self.assertEqual(set(kwargs['env']), {'PATH', 'LANG', 'LC_ALL', 'XDG_RUNTIME_DIR'})

    def test_process_timeout_or_nonzero_is_failure(self):
        info = type('Stat', (), {'st_mode': 0o40700, 'st_uid': 1000})()
        for effect in [subprocess.TimeoutExpired('wpctl', 4), FileNotFoundError(),
                       subprocess.CompletedProcess([], 1, stdout='', stderr='permission denied')]:
            with self.subTest(effect=effect), patch.object(server.os, 'getuid', return_value=1000, create=True), \
                    patch.object(server.Path, 'lstat', return_value=info), \
                    patch.object(server.subprocess, 'run') as run:
                if isinstance(effect, Exception):
                    run.side_effect = effect
                else:
                    run.return_value = effect
                with self.assertRaises(server.ToolError):
                    server.run_wpctl(['set-volume', server.SINK, '50%'])

    def test_wrong_runtime_owner_is_rejected(self):
        info = type('Stat', (), {'st_mode': 0o40700, 'st_uid': 999})()
        with patch.object(server.os, 'getuid', return_value=1000, create=True), \
                patch.object(server.Path, 'lstat', return_value=info), \
                patch.object(server.subprocess, 'run') as run:
            with self.assertRaises(server.ToolError):
                server.run_wpctl(['get-volume', server.SINK])
            run.assert_not_called()


class RpcTests(unittest.TestCase):
    def test_handshake_and_list_no_execution(self):
        runner = FakeRunner([])
        tools = server.VolumeTools(runner)
        result = server.handle({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize'}, tools)
        self.assertEqual(result['result']['protocolVersion'], '2024-11-05')
        result = server.handle({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'}, tools)
        self.assertEqual({item['name'] for item in result['result']['tools']}, {'set_volume', 'get_volume'})
        self.assertEqual(runner.calls, [])

    def test_notification_cannot_execute(self):
        runner = FakeRunner([])
        result = server.handle({'jsonrpc': '2.0', 'method': 'tools/call',
                                'params': {'name': 'set_volume', 'arguments': {'percent': 50}}},
                               server.VolumeTools(runner))
        self.assertIsNone(result)
        self.assertEqual(runner.calls, [])

    def test_execution_error_is_rpc_error_for_current_kylinbot(self):
        runner = FakeRunner([server.ToolError('setter failed')])
        result = server.handle({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
                                'params': {'name': 'set_volume', 'arguments': {'percent': 50}}},
                               server.VolumeTools(runner))
        self.assertNotIn('result', result)
        self.assertEqual(result['error']['code'], -32010)

    def test_stream_recovers_from_bad_json_and_large_request(self):
        valid = json.dumps({'jsonrpc': '2.0', 'id': 4, 'method': 'ping'})
        source = io.StringIO('{bad json}\n' + 'x' * (server.MAX_LINE + 4) + '\n' + valid + '\n')
        target = io.StringIO()
        server.serve(source, target, server.VolumeTools(FakeRunner([])))
        results = [json.loads(line) for line in target.getvalue().splitlines()]
        self.assertEqual([results[0]['error']['code'], results[1]['error']['code']], [-32700, -32600])
        self.assertEqual(results[2]['result'], {})

    def test_bad_rpc_inputs_are_rejected(self):
        for message in [[], {'id': 1}, {'jsonrpc': '2.0', 'id': True, 'method': 'ping'},
                        {'jsonrpc': '2.0', 'id': 1, 'method': 'ping', 'params': []}]:
            self.assertIn('error', server.handle(message, server.VolumeTools(FakeRunner([]))))


if __name__ == '__main__':
    unittest.main()
