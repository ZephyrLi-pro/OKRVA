"""Explicit intent routing must keep execution/authorization in KylinBot/MCP."""
import copy
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import threading
import unittest
import urllib.error
import urllib.request
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
from riscv_ai_adapter.backends import UpstreamResponse, explicit_system_intent
from riscv_ai_adapter.config import AdapterConfig, BackendConfig, ServerConfig
from riscv_ai_adapter.server import make_server
from test_voice_system_route import NAMES, PREFIX, QUERY, native, payload


def intent(value, allowed=NAMES, enabled=True):
    return explicit_system_intent(json.dumps(value, ensure_ascii=False).encode(), allowed, enabled)


class SystemIntentTests(unittest.TestCase):
    def test_explicit_actions_with_or_without_voice_format(self):
        for prefix in ('', PREFIX, '[2026-10-22 12:00:00 +08:00] ' + PREFIX):
            for query, name, arguments in [
                    ('音量设置成50%', 'sys__set_volume', {'percent': 50}),
                    (QUERY, 'sys__adjust_volume', {'delta': 10}),
                    ('将音量调整到50%，然后打开文件管理器。', 'sys__set_volume', {'percent': 50}),
                    ('把音量调小20个百分点', 'sys__adjust_volume', {'delta': -20}),
                    ('取消静音', 'sys__set_mute', {'muted': False}),
                    ('打开系统设置', 'sys__open_application', {'application': 'system_settings'})]:
                value = payload(prefix + query)
                before = copy.deepcopy(value)
                self.assertEqual(intent(value), {'name': name, 'arguments': arguments})
                self.assertEqual(before, value)

    def chain(self):
        return native(payload(PREFIX + QUERY), 'sys__adjust_volume', {'delta': 10},
                      '系统音量已设置为45%，已回读确认。', 'volume-id')

    def test_second_step_uses_native_success_not_assistant_claim(self):
        value = self.chain()
        value['messages'][-2]['content'] = '系统设置已打开。sys__open_application(...)'
        before = copy.deepcopy(value)
        self.assertEqual(intent(value), {'name': 'sys__open_application', 'arguments': {'application': 'system_settings'}})
        self.assertEqual(value, before)

    def test_summary_copies_both_real_readbacks_never_requested_target(self):
        value = native(payload(PREFIX + '音量设置成50%，然后打开文件管理器'),
                       'sys__set_volume', {'percent': 50}, '工具实际回读45%。', 'volume-id')
        native(value, 'sys__open_application', {'application': 'file_manager'}, '文件管理器窗口已检测到。', 'app-id')
        self.assertEqual(intent(value), {'content': '工具实际回读45%。文件管理器窗口已检测到。'})

    def test_standalone_summary_requires_matching_parameters_and_real_result(self):
        value = native(payload('音量设置成35%'), 'sys__set_volume', {'percent': 35}, '实际35%。', 'v')
        self.assertEqual(intent(value), {'content': '实际35%。'})
        value['messages'][-2]['tool_calls'][0]['function']['arguments'] = '{"percent":50}'
        result = intent(value)
        self.assertNotIn('name', result)
        self.assertIn('未确认', result['content'])

    def test_failures_and_malformed_results_stop_no_retry(self):
        for content in ['Error: denied', 'sys__open_application(system_settings)',
                        json.dumps({'content': [{'type': 'text', 'text': 'denied'}], 'isError': True}),
                        json.dumps({'content': [None], 'isError': False}),
                        json.dumps({'content': [{'type': 'text', 'text': ''}], 'isError': False}),
                        json.dumps({'content': [{'type': 'text', 'text': 'x' * 2001}], 'isError': False})]:
            value = self.chain()
            value['messages'][-1]['content'] = content
            result = intent(value)
            self.assertNotIn('name', result)
            self.assertIn('已停止后续操作', result['content'])

    def test_broken_ids_arguments_and_extra_frames_stop(self):
        for variation in ('id', 'missing_id', 'function', 'args', 'bool_args', 'missing_result', 'plain_assistant', 'extra_pair'):
            value = self.chain()
            call = value['messages'][-2]['tool_calls'][0]
            if variation == 'id': value['messages'][-1]['tool_call_id'] = 'other'
            if variation == 'missing_id': call['id'] = ''
            if variation == 'function': call['function']['name'] = 'shell'
            if variation == 'args': call['function']['arguments'] = '{"delta":100}'
            if variation == 'bool_args': call['function']['arguments'] = '{"delta":true}'
            if variation == 'missing_result': value['messages'].pop()
            if variation == 'plain_assistant': value['messages'][-2].pop('tool_calls')
            if variation == 'extra_pair': value['messages'] += copy.deepcopy(value['messages'][-2:]) * 2
            result = intent(value)
            self.assertNotIn('name', result, variation)
            self.assertIn('已停止后续操作', result['content'], variation)

    def test_duplicate_native_ids_stop_and_preserve_first_readback(self):
        value = self.chain()
        native(value, 'sys__open_application', {'application': 'system_settings'}, '系统设置窗口已检测到。', 'volume-id')
        result = intent(value)
        self.assertIn('45%', result['content'])
        self.assertNotIn('系统设置窗口已检测到', result['content'])
        self.assertNotIn('name', result)

    def test_unavailable_or_incompatible_schema_opts_out_without_invention(self):
        for variation in ('missing', 'duplicate', 'required_extra', 'required_bad', 'type', 'enum', 'range', 'unknown_constraint'):
            value = payload(PREFIX + QUERY)
            tool = next(t for t in value['tools'] if t['function']['name'] == 'sys__adjust_volume')
            params = tool['function']['parameters']
            if variation == 'missing': value['tools'].remove(tool)
            if variation == 'duplicate': value['tools'].append(copy.deepcopy(tool))
            if variation == 'required_extra': params['required'].append('not_provided')
            if variation == 'required_bad': params['required'].append({})
            if variation == 'type': params['properties']['delta']['type'] = 'string'
            if variation == 'enum': params['properties']['delta']['enum'] = [20]
            if variation == 'range': params['properties']['delta']['maximum'] = 5
            if variation == 'unknown_constraint': params['properties']['delta']['const'] = 20
            self.assertIsNone(intent(value), variation)
        self.assertIsNone(intent(payload(PREFIX + QUERY), allowed=('sys__adjust_volume',)))
        self.assertIsNone(intent(payload(PREFIX + QUERY), enabled=False))

    def test_quotes_negations_references_multimodal_and_general_tasks_opt_out(self):
        for content in ['你是谁', '不要' + QUERY, '翻译：' + QUERY, PREFIX + '不要' + QUERY,
                        PREFIX + '解释“' + QUERY + '”', PREFIX + '按刚才说的' + QUERY,
                        '用户问题：' + QUERY, '日志\n' + PREFIX + QUERY, PREFIX + PREFIX + QUERY,
                        PREFIX + '把音量调大一点，然后' + PREFIX + '打开系统设置',
                        PREFIX + QUERY + ' [IMAGE:xxx]', '把音量设成1000%',
                        PREFIX + QUERY + '再打开文件管理器',
                        [{'type': 'text', 'text': QUERY}, {'type': 'image_url', 'image_url': {'url': 'data:image/png;base64,aA=='}}]]:
            self.assertIsNone(intent(payload(content)), str(content))

    def test_caller_forced_choices_and_invalid_stream_opt_out(self):
        for choice in ('required', 'none', {'type': 'function', 'function': {'name': 'shell'}}):
            value = payload(QUERY)
            value['tool_choice'] = choice
            self.assertIsNone(intent(value))
        value = payload(QUERY); value['stream'] = 'true'
        self.assertIsNone(intent(value))

    def test_prose_cannot_become_a_native_call_or_replay_completed_action(self):
        value = payload(QUERY)
        value['messages'].append({'role': 'assistant', 'content': 'sys__set_volume(percent=100)'})
        self.assertNotIn('name', intent(value))
        value = self.chain()
        value['messages'].append({'role': 'user', 'content': '谢谢'})
        self.assertIsNone(intent(value))


class SystemIntentHTTPTests(unittest.TestCase):
    def test_authorization_priority_and_general_inference_are_not_bypassed(self):
        config = AdapterConfig(server=ServerConfig(port=0, token_env='RVAI_INTENT_TEST_TOKEN'), resource_groups={},
            backends=(BackendConfig(name='intent', capability='chat', url='http://127.0.0.1:1/v1/chat/completions',
                models=('test',), allowed_tools=NAMES, system_prompt_override='本地系统助手。'),))
        app = make_server(config)
        threading.Thread(target=app.serve_forever, daemon=True).start()
        try:
            with patch.dict(os.environ, {'RVAI_INTENT_TEST_TOKEN': 'synthetic-test-only'}):
                for authorization, priority, status in [('', '50', 401), ('Bearer incorrect', '50', 401),
                        ('Bearer synthetic-test-only', '101', 400), ('Bearer synthetic-test-only', 'bad', 400)]:
                    value = payload(QUERY); value['model'] = 'test'
                    req = urllib.request.Request(f'http://127.0.0.1:{app.server_port}/v1/chat/completions',
                        data=json.dumps(value).encode(), headers={'Content-Type': 'application/json',
                        'Authorization': authorization, 'X-RVAI-Priority': priority})
                    with patch.object(app.registry, 'open', side_effect=AssertionError('Must reject first')):
                        with self.assertRaises(urllib.error.HTTPError) as caught:
                            urllib.request.urlopen(req, timeout=2)
                        self.assertEqual(caught.exception.code, status)
                        caught.exception.close()
                received = []

                @contextlib.contextmanager
                def upstream(_backend, body, *_args):
                    received.append(json.loads(body))
                    wire = b'{"choices":[{"message":{"role":"assistant","content":"model reply"}}]}'
                    yield UpstreamResponse(200, {'Content-Type': 'application/json', 'Content-Length': str(len(wire))}, io.BytesIO(wire))

                value = payload('请分析这份错误日志，不要修改系统'); value.update(model='test', stream=False)
                req = urllib.request.Request(f'http://127.0.0.1:{app.server_port}/v1/chat/completions',
                    data=json.dumps(value).encode(), headers={'Content-Type': 'application/json', 'Authorization': 'Bearer synthetic-test-only'})
                with patch.object(app.registry, 'open', upstream):
                    with urllib.request.urlopen(req, timeout=2) as response:
                        self.assertEqual(json.load(response)['choices'][0]['message']['content'], 'model reply')
                self.assertEqual(len(received), 1)
                self.assertEqual(received[0]['messages'][-1]['content'], value['messages'][-1]['content'])
        finally:
            app.shutdown()
            app.server_close()

    def test_json_and_sse_native_cycle_never_run_inference_or_executor(self):
        config = AdapterConfig(server=ServerConfig(port=0), resource_groups={}, backends=(BackendConfig(
            name='intent', capability='chat', url='http://127.0.0.1:1/v1/chat/completions',
            models=('test',), allowed_tools=NAMES, system_prompt_override='本地系统助手。'),))
        app = make_server(config)
        threading.Thread(target=app.serve_forever, daemon=True).start()
        try:
            with patch.object(app.registry, 'open', side_effect=AssertionError('Must not infer or execute')):
                for stream in (False, True):
                    value = payload(PREFIX + QUERY)
                    value.update(model='test', stream=stream)

                    def request():
                        req = urllib.request.Request(f'http://127.0.0.1:{app.server_port}/v1/chat/completions',
                            data=json.dumps(value).encode(), headers={'Content-Type': 'application/json'})
                        with urllib.request.urlopen(req, timeout=2) as response:
                            wire = response.read()
                        if not stream:
                            reply = json.loads(wire)
                            self.assertEqual(reply['rvai_route'], 'explicit-system-intent')
                            return reply['choices'][0]['message']
                        frames = [json.loads(line[6:]) for line in wire.decode().splitlines() if line.startswith('data: {')]
                        self.assertTrue(wire.endswith(b'data: [DONE]\n\n'))
                        self.assertEqual(len(frames), 2)
                        return frames[0]['choices'][0]['delta']

                    first = request()['tool_calls'][0]
                    self.assertEqual(first['function']['name'], 'sys__adjust_volume')
                    native(value, first['function']['name'], json.loads(first['function']['arguments']),
                           '系统音量已设置为45%，已回读确认。', first['id'])
                    second = request()['tool_calls'][0]
                    self.assertNotEqual(first['id'], second['id'])
                    self.assertEqual(second['function']['name'], 'sys__open_application')
                    native(value, second['function']['name'], json.loads(second['function']['arguments']),
                           '系统设置窗口已检测到。', second['id'])
                    result = request()
                    self.assertNotIn('tool_calls', result)
                    self.assertEqual(result['content'], '系统音量已设置为45%，已回读确认。系统设置窗口已检测到。')
        finally:
            app.shutdown()
            app.server_close()


if __name__ == '__main__':
    unittest.main()
