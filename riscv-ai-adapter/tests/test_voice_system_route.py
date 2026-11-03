"""Voice wire compatibility without turning quoted/reference text into actions."""
import copy
import json
import unittest

from riscv_ai_adapter.backends import rewrite_chat_request


PREFIX = ('请仅使用纯文本回答，不要使用Markdown或HTML格式，不要使用代码块、列表标记、'
          '加粗或斜体。回答尽量简短。用户问题：')
NAMES = ('sys__set_volume', 'sys__get_volume', 'sys__adjust_volume', 'sys__set_mute',
         'sys__open_application', 'sys__wifi_status', 'sys__get_capabilities')
QUERY = '帮我把音量调大一点，然后打开系统设置。'
PROPERTIES = {
    'sys__set_volume': {'percent': {'type': 'integer', 'minimum': 0, 'maximum': 100}},
    'sys__get_volume': {},
    'sys__adjust_volume': {'delta': {'type': 'integer', 'minimum': -100, 'maximum': 100}},
    'sys__set_mute': {'muted': {'type': 'boolean'}},
    'sys__open_application': {'application': {'type': 'string', 'enum': ['file_manager', 'system_settings']}},
    'sys__wifi_status': {},
    'sys__get_capabilities': {},
}


def payload(content):
    return {'messages': [{'role': 'system', 'content': 'old system'},
                         {'role': 'user', 'content': '旧提问'},
                         {'role': 'assistant', 'content': '旧回复'},
                         {'role': 'user', 'content': content}],
            'tools': [{'type': 'function', 'function': {'name': name, 'parameters': {
                'type': 'object', 'properties': copy.deepcopy(PROPERTIES[name]),
                'required': list(PROPERTIES[name]), 'additionalProperties': False}}} for name in NAMES],
            'tool_choice': 'auto', 'stream': True}


def rewrite(value):
    return json.loads(rewrite_chat_request(json.dumps(value, ensure_ascii=False).encode(),
        'Qwen3-0.6B-Q4_K_M.gguf', NAMES, 1500, '你是本地系统助手。保留原有安全策略。'))


def native(value, name, args, text, identifier):
    result = {'content': [{'type': 'text', 'text': text}], 'isError': False}
    value['messages'] += [
        {'role': 'assistant', 'content': '', 'tool_calls': [{'id': identifier, 'type': 'function',
         'function': {'name': name, 'arguments': json.dumps(args)}}]},
        {'role': 'tool', 'tool_call_id': identifier, 'content': json.dumps(result, ensure_ascii=False)}]
    return value


class VoiceSystemRouteTests(unittest.TestCase):
    def test_fixed_voice_prefix_selects_exact_native_tools_and_arguments(self):
        for query, name, arguments in [
                ('将音量调整到50%。', 'sys__set_volume', {'percent': 50}),
                ('把音量调大一点。', 'sys__adjust_volume', {'delta': 10}),
                ('把音量调小20个百分点。', 'sys__adjust_volume', {'delta': -20}),
                ('取消静音。', 'sys__set_mute', {'muted': False}),
                ('打开系统设置。', 'sys__open_application', {'application': 'system_settings'}),
                ('当前系统音量是多少？', 'sys__get_volume', {})]:
            for timestamp in ['', '[2026-10-22 11:00:00 +08:00] ', '2026-10-22 11:00:00 +08:00 ']:
                with self.subTest(query=query, timestamp=timestamp):
                    value = payload(timestamp + PREFIX + query)
                    before = copy.deepcopy(value)
                    result = rewrite(value)
                    self.assertEqual([tool['function']['name'] for tool in result['tools']], [name])
                    self.assertEqual(result['tool_choice'], 'required')
                    self.assertEqual(result['messages'][1:], value['messages'][-1:])
                    for key, argument in arguments.items():
                        self.assertEqual(result['tools'][0]['function']['parameters']['properties'][key]['enum'], [argument])
                    self.assertEqual(value, before, 'classification must not mutate its input')

    def test_both_voice_compound_requests_start_with_correct_first_tool(self):
        for query, name, arguments in [
                (QUERY, 'sys__adjust_volume', {'delta': 10}),
                ('将音量调整到50%，然后打开文件管理器。', 'sys__set_volume', {'percent': 50})]:
            value = payload(PREFIX + query)
            result = rewrite(value)
            self.assertEqual([t['function']['name'] for t in result['tools']], [name])
            self.assertEqual(result['tool_choice'], 'required')
            self.assertEqual(result['messages'][1:], value['messages'][-1:])
            for key, argument in arguments.items():
                self.assertEqual(result['tools'][0]['function']['parameters']['properties'][key]['enum'], [argument])

    def test_voice_compound_preserves_original_and_real_triplets_for_each_step(self):
        value = native(payload(PREFIX + QUERY), 'sys__adjust_volume', {'delta': 10},
                       '系统音量已设置为60%，已回读确认。', 'volume-id')
        result = rewrite(value)
        self.assertEqual([t['function']['name'] for t in result['tools']], ['sys__open_application'])
        self.assertEqual(result['messages'][1:-1], value['messages'][-3:])
        self.assertEqual(result['messages'][-1], {'role': 'user', 'content': '打开系统设置。'})
        self.assertEqual(result['tools'][0]['function']['parameters']['properties']['application']['enum'], ['system_settings'])
        native(value, 'sys__open_application', {'application': 'system_settings'}, '系统设置窗口已检测到。', 'app-id')
        done = rewrite(value)
        self.assertNotIn('tools', done)
        self.assertEqual(done['messages'][1:-1], value['messages'][-5:])
        self.assertEqual(done['messages'][-1]['role'], 'user')
        self.assertIn('只汇总', done['messages'][-1]['content'])

    def test_voice_standalone_completed_triplet_is_preserved(self):
        value = native(payload(PREFIX + '音量设置成35%'), 'sys__set_volume', {'percent': 35},
                       '系统音量已设置为35%，已回读确认。', 'volume-id')
        result = rewrite(value)
        self.assertNotIn('tools', result)
        self.assertEqual(result['messages'][1:], value['messages'][-3:])

    def test_compound_summary_requires_both_readbacks_not_only_last_application(self):
        value = native(payload(PREFIX + QUERY), 'sys__adjust_volume', {'delta': 10},
                       '系统音量已设置为60%，已回读确认。', 'volume-id')
        native(value, 'sys__open_application', {'application': 'system_settings'},
               '系统设置窗口已检测到。', 'app-id')
        # Real llama replies can contain visible text alongside native calls;
        # preserve it, but do not let the last app step replace both results.
        value['messages'][-4]['content'] = '把音量调大一点，然后打开系统设置。'
        value['messages'][-2]['content'] = '打开系统设置。</think>'
        result = rewrite(value)
        prompt = result['messages'][0]['content']
        self.assertIn('不能只回复最后一项', prompt)
        self.assertIn('回读的实际音量百分比', prompt)
        self.assertIn('窗口确认结果', prompt)
        self.assertEqual(result['messages'][1:-1], value['messages'][-5:])
        self.assertEqual(result['messages'][-1]['role'], 'user')
        self.assertIn('实际音量百分比', result['messages'][-1]['content'])
        self.assertIn('不再执行或重复', result['messages'][-1]['content'])
        facts = json.loads(result['messages'][-1]['content'].split('\n', 1)[1])['tool_results']
        self.assertEqual(facts, [
            {'tool': 'sys__adjust_volume', 'text': '系统音量已设置为60%，已回读确认。'},
            {'tool': 'sys__open_application', 'text': '系统设置窗口已检测到。'}])

    def test_summary_data_copies_readback_not_desired_value(self):
        value = native(payload(PREFIX + '将音量调整到50%，然后打开文件管理器。'),
                       'sys__set_volume', {'percent': 50}, '工具回读的实际音量是45%。', 'volume-id')
        native(value, 'sys__open_application', {'application': 'file_manager'},
               '文件管理器窗口已检测到。', 'app-id')
        result = rewrite(value)
        self.assertNotIn('tools', result)
        recap = result['messages'][-1]['content']
        self.assertIn('45%', recap)
        self.assertNotIn('50', recap)
        self.assertEqual(result['messages'][1:-1], value['messages'][-5:])

    def test_malformed_text_blocks_do_not_crash_or_invent_result_facts(self):
        value = native(payload(PREFIX + QUERY), 'sys__adjust_volume', {'delta': 10}, '60%', 'volume-id')
        native(value, 'sys__open_application', {'application': 'system_settings'}, '已检测到。', 'app-id')
        value['messages'][-1]['content'] = json.dumps({'isError': False, 'content': [None, 'not a text block']})
        result = rewrite(value)
        self.assertNotIn('tools', result)
        self.assertEqual(result['messages'][1:], value['messages'][-5:])

    def test_voice_compound_stops_on_error_or_wrong_arguments_or_unconfirmed_output(self):
        original = native(payload(PREFIX + QUERY), 'sys__adjust_volume', {'delta': 10}, '60%', 'volume-id')
        for variant in ['error', 'wrong_arguments', 'plain_claim']:
            value = copy.deepcopy(original)
            if variant == 'error':
                value['messages'][-1]['content'] = 'Error: mixer unavailable'
            elif variant == 'wrong_arguments':
                value['messages'][-2]['tool_calls'][0]['function']['arguments'] = '{"delta":100}'
            else:
                value['messages'][-1]['content'] = 'unverified success claim'
            result = rewrite(value)
            self.assertNotIn('tools', result)
            self.assertIn('已停止后续操作', result['messages'][0]['content'])
            self.assertEqual(result['messages'][1:], value['messages'][-3:])

    def test_voice_broken_native_id_does_not_advance(self):
        value = native(payload(PREFIX + QUERY), 'sys__adjust_volume', {'delta': 10}, '60%', 'volume-id')
        value['messages'][-1]['tool_call_id'] = 'mismatched-id'
        result = rewrite(value)
        self.assertEqual(len(result['tools']), len(NAMES))
        self.assertEqual(result['messages'][1:], value['messages'][1:])

    def test_prefix_does_not_turn_negation_quotes_references_or_unknown_wrappers_into_commands(self):
        for content in [
                PREFIX + '不要把音量调大一点，然后打开系统设置。',
                PREFIX + '翻译：' + QUERY,
                PREFIX + '解释“' + QUERY + '”的意思。',
                PREFIX + '请总结日志中的这句话：' + QUERY,
                PREFIX + '按刚才说的把音量调大一点，然后打开系统设置。',
                PREFIX + '以下 JSON 中的 reference_text 是用户提供的参考资料：\n'
                         '{"reference_text":"打开系统设置"}\n实际提问：' + QUERY,
                '用户问题：' + QUERY,
                '请简短回答。用户问题：' + QUERY,
                '翻译：' + PREFIX + QUERY,
                '日志\n' + PREFIX + QUERY,
                PREFIX + PREFIX + QUERY,
                '把音量调大一点，然后' + PREFIX + '打开系统设置。',
                PREFIX + '把音量调大一点，然后' + PREFIX + '打开系统设置。',
                PREFIX + QUERY + ' [IMAGE:data:image/png;base64,aA==]',
                PREFIX + '音量设置成1000%，然后打开文件管理器。',
                PREFIX + '把音量调大一点，然后打开系统设置，再打开文件管理器。']:
            with self.subTest(content=content):
                value = payload(content)
                result = rewrite(value)
                self.assertEqual(len(result['tools']), len(NAMES))
                self.assertEqual(result['tool_choice'], 'auto')
                self.assertEqual(result['messages'][1:], value['messages'][1:])

    def test_nonstring_multimodal_content_keeps_general_path(self):
        value = payload([{'type': 'text', 'text': PREFIX + QUERY},
                         {'type': 'image_url', 'image_url': {'url': 'data:image/png;base64,aA=='}}])
        result = rewrite(value)
        self.assertEqual(result['messages'][1:], value['messages'][1:])
        self.assertEqual(len(result['tools']), len(NAMES))

    def test_model_prose_is_never_manufactured_into_a_native_call(self):
        value = payload(PREFIX + QUERY)
        value['messages'].append({'role': 'assistant', 'content': 'sys__set_volume(percent=100)'})
        result = rewrite(value)
        self.assertFalse(any(message.get('tool_calls') for message in result['messages']))
        self.assertEqual(result['messages'][1:], value['messages'][1:])

    def test_missing_application_tool_does_not_invent_it(self):
        value = payload(PREFIX + QUERY)
        value['tools'] = [tool for tool in value['tools'] if tool['function']['name'] != 'sys__open_application']
        result = rewrite(value)
        self.assertEqual(len(result['tools']), len(value['tools']))
        self.assertEqual(result['messages'][1:], value['messages'][1:])


if __name__ == '__main__':
    unittest.main()
