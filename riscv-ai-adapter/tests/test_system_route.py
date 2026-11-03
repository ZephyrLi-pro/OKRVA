import json
import unittest
from riscv_ai_adapter.backends import rewrite_chat_request

NAMES = ('shell', 'file_read', 'file_write', 'glob_search', 'sys__set_volume', 'sys__get_volume',
         'sys__adjust_volume', 'sys__set_mute', 'sys__open_application', 'sys__wifi_status',
         'sys__wifi_scan', 'sys__bluetooth_status', 'sys__get_brightness', 'sys__get_capabilities')


def payload(text):
    return {'model': 'local', 'messages': [{'role': 'system', 'content': 'system'},
            {'role': 'user', 'content': '旧提问'}, {'role': 'assistant', 'content': '旧错误回复'},
            {'role': 'user', 'content': text}],
            'tools': [{'type': 'function', 'function': {'name': name, 'parameters': {'type': 'object'}}} for name in NAMES],
            'stream': True, 'tool_choice': 'auto'}


def rewrite(value):
    return json.loads(rewrite_chat_request(json.dumps(value, ensure_ascii=False).encode(),
                      'Qwen3-0.6B-Q4_K_M.gguf', NAMES, 1500, '你是本地系统助手。原有通用策略。'))


class SystemRouteTests(unittest.TestCase):
    def test_exact_new_standalone_tools(self):
        for query, name in [('把音量调大一点', 'adjust_volume'), ('请帮我把音量调小10个百分点', 'adjust_volume'),
                ('取消静音', 'set_mute'), ('打开文件管理器', 'open_application'), ('打开系统设置', 'open_application'),
                ('当前WiFi状态', 'wifi_status'), ('列出附近的WiFi', 'wifi_scan'), ('当前蓝牙状态', 'bluetooth_status'),
                ('当前屏幕亮度是多少', 'get_brightness'), ('当前支持哪些系统操作', 'get_capabilities')]:
            with self.subTest(query=query):
                result = rewrite(payload(query))
                self.assertEqual([t['function']['name'] for t in result['tools']], ['sys__' + name])
                self.assertEqual(result['tool_choice'], 'required')
                self.assertEqual(result['messages'][1:], payload(query)['messages'][-1:])

    def test_negated_quoted_contextual_compound_and_unavailable_actions_stay_general(self):
        for query in ['不要打开系统设置', '解释取消静音是什么意思', '翻译：打开文件管理器',
                '按刚才说的把音量调大一点', '不要把音量设成50%，然后打开文件管理器',
                '翻译：将音量调整到50%，然后打开文件管理器',
                '帮我把音量调大一点，然后打开设置，再打开文件管理器',
                '关闭WiFi', '把屏幕亮度改为60%', '查看日志里的蓝牙状态']:
            result = rewrite(payload(query))
            self.assertEqual(len(result['tools']), len(NAMES), query)
            self.assertEqual(result['messages'][1:], payload(query)['messages'][1:], query)

    def completed(self, query='音量设置成35%', name='sys__set_volume'):
        value = payload(query)
        value['messages'] += [{'role': 'assistant', 'content': '', 'tool_calls': [
            {'id': 'real-call', 'type': 'function', 'function': {'name': name, 'arguments': '{"percent":35}'}}]},
            {'role': 'tool', 'tool_call_id': 'real-call', 'content': '系统音量已设置为35%，已回读确认。'}]
        return value

    def test_completed_standalone_preserves_native_triplet_and_removes_catalog(self):
        value = self.completed()
        result = rewrite(value)
        self.assertEqual(result['messages'][1:], value['messages'][-3:])
        self.assertNotIn('tools', result)
        self.assertNotIn('tool_choice', result)
        self.assertLessEqual(result['max_tokens'], 128)

    def test_tool_failure_is_preserved_in_final_route(self):
        value = self.completed()
        value['messages'][-1]['content'] = 'Error: setter failed, actual volume is still50%'
        result = rewrite(value)
        self.assertEqual(result['messages'][-1], value['messages'][-1])
        self.assertIn('失败', result['messages'][0]['content'])

    def test_missing_id_mismatched_tool_multiple_calls_or_compound_not_finalized(self):
        variants = []
        value = self.completed(); value['messages'][-1]['tool_call_id'] = 'wrong'; variants.append(value)
        value = self.completed(); value['messages'][-2]['tool_calls'][0]['id'] = ''; variants.append(value)
        value = self.completed(name='shell'); variants.append(value)
        value = self.completed(); value['messages'][-2]['tool_calls'] *= 2; variants.append(value)
        value = self.completed(); value['tool_choice'] = 'required'; variants.append(value)
        for value in variants:
            result = rewrite(value)
            self.assertEqual(len(result['tools']), len(NAMES))
            self.assertEqual(result['messages'][1:], value['messages'][1:])

    def with_call(self, value, name, arguments, text, identifier='real-call'):
        content = text if text.startswith('Error:') else json.dumps({'content': [{'type': 'text', 'text': text}], 'isError': False})
        value['messages'] += [{'role': 'assistant', 'content': '', 'tool_calls': [
            {'id': identifier, 'type': 'function', 'function': {'name': name, 'arguments': json.dumps(arguments)}}]},
            {'role': 'tool', 'tool_call_id': identifier, 'content': content}]
        return value

    def test_explicit_two_step_sequence_uses_next_native_tool_not_general_catalog(self):
        for query, first in [('将音量调整到50%，然后打开文件管理器', 'sys__set_volume'),
                             ('帮我把音量调大一点，然后打开系统设置', 'sys__adjust_volume')]:
            value = payload(query)
            result = rewrite(value)
            self.assertEqual([t['function']['name'] for t in result['tools']], [first])
            self.assertEqual(result['tool_choice'], 'required')
            self.assertEqual(result['messages'][1:], value['messages'][-1:])
        value = self.with_call(payload('音量设置成35%，然后打开文件管理器'),
                               'sys__set_volume', {'percent': 35}, '系统音量已设置为35%，已回读确认。')
        result = rewrite(value)
        self.assertEqual([t['function']['name'] for t in result['tools']], ['sys__open_application'])
        self.assertEqual(result['messages'][1:-1], value['messages'][-3:])
        self.assertEqual(result['messages'][-1], {'role': 'user', 'content': '打开文件管理器'})
        self.assertIn('不要重复', result['messages'][0]['content'])
        self.with_call(value, 'sys__open_application', {'application': 'file_manager'}, '文件管理器窗口已检测到。', 'app-call')
        result = rewrite(value)
        self.assertNotIn('tools', result)
        self.assertEqual(result['messages'][1:], value['messages'][-5:])

    def test_sequence_stops_after_error_or_wrong_arguments_no_implicit_retry(self):
        for arguments, text in [({'percent': 35}, 'Error: volume failed'),
                                 ({'percent': 50}, '工具实际设置为50%')]:
            value = self.with_call(payload('音量设置成35%，然后打开文件管理器'),
                'sys__set_volume', arguments, text)
            result = rewrite(value)
            self.assertNotIn('tools', result)
            self.assertIn('已停止后续操作', result['messages'][0]['content'])
            self.assertEqual(result['messages'][1:], value['messages'][-3:])

    def test_sequence_missing_schema_or_broken_chain_does_not_invent_or_advance(self):
        value = payload('音量设置成35%，然后打开文件管理器')
        value['tools'] = [t for t in value['tools'] if t['function']['name'] != 'sys__open_application']
        self.assertEqual(len(rewrite(value)['tools']), len(value['tools']))
        value = self.with_call(payload('音量设置成35%，然后打开文件管理器'),
                               'sys__set_volume', {'percent': 35}, '35%')
        value['messages'][-1]['tool_call_id'] = 'wrong'
        self.assertEqual(len(rewrite(value)['tools']), len(NAMES))

    def test_sequence_unconfirmed_plain_text_cannot_advance_to_next_action(self):
        value = self.with_call(payload('音量设置成35%，然后打开文件管理器'),
                               'sys__set_volume', {'percent': 35}, '35%')
        value['messages'][-1]['content'] = 'unverified success claim'
        result = rewrite(value)
        self.assertNotIn('tools', result)
        self.assertIn('已停止后续操作', result['messages'][0]['content'])

    def test_exact_parameters_constrain_schema_without_manufacturing_tool_call(self):
        for query, name, key, expected in [('音量设置成50%', 'set_volume', 'percent', 50),
                ('把音量调小 20 个百分点', 'adjust_volume', 'delta', -20),
                ('取消静音', 'set_mute', 'muted', False),
                ('打开系统设置', 'open_application', 'application', 'system_settings')]:
            value = payload(query)
            next(t for t in value['tools'] if t['function']['name'] == 'sys__' + name)['function']['parameters'] = {
                'type': 'object', 'properties': {key: {'type': 'string' if isinstance(expected, str) else
                     'boolean' if type(expected) is bool else 'integer'}}, 'required': [key]}
            result = rewrite(value)
            self.assertEqual(result['tools'][0]['function']['parameters']['properties'][key]['enum'], [expected])
            self.assertFalse(any('tool_calls' in m for m in result['messages']))
            self.assertNotIn('enum', value['tools'][NAMES.index('sys__' + name)]['function']['parameters']['properties'][key])


if __name__ == '__main__':
    unittest.main()
