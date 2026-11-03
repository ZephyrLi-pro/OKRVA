import json
import unittest
from riscv_ai_adapter.backends import rewrite_chat_request

NAMES = ('shell', 'file_read', 'file_write', 'glob_search', 'sys__set_volume', 'sys__get_volume')


def payload(text):
    return {'model': 'local', 'messages': [{'role': 'system', 'content': 'original system'},
            {'role': 'user', 'content': '保留历史'}, {'role': 'assistant', 'content': '好的'},
            {'role': 'user', 'content': text}],
            'tools': [{'type': 'function', 'function': {'name': name, 'parameters': {'type': 'object'}}}
                      for name in NAMES], 'stream': True, 'temperature': 0.7, 'tool_choice': 'auto'}


def rewrite(value):
    return json.loads(rewrite_chat_request(json.dumps(value, ensure_ascii=False).encode(),
                      'Qwen3-0.6B-Q4_K_M.gguf', NAMES, 1500, '你是本地系统助手。原有通用策略。'))


class VolumeRouteTests(unittest.TestCase):
    def test_exact_setters_get_one_real_tool(self):
        for query in ['音量设置成50%', '将音量调整到50%', '把系统音量设置为50%', '请帮我把音量调到0%',
                      '[2026-10-21 16:00:00 +08:00] 音量设置成100%']:
            with self.subTest(query=query):
                value = rewrite(payload(query))
                self.assertEqual([tool['function']['name'] for tool in value['tools']], ['sys__set_volume'])
                self.assertEqual(value['tool_choice'], 'required')
                self.assertEqual(value['temperature'], 0.2)
                self.assertFalse(value['parallel_tool_calls'])
                self.assertEqual(value['messages'][1:], payload(query)['messages'][-1:])
                self.assertLessEqual(value['max_tokens'], 96)

    def test_exact_getters_are_read_only(self):
        for query in ['查询当前系统音量是多少，不要修改。', '当前音量是多少？', '查看现在音量多大']:
            value = rewrite(payload(query))
            self.assertEqual(value['tools'][0]['function']['name'], 'sys__get_volume')
            self.assertEqual(len(value['tools']), 1)
            self.assertEqual(value['messages'][1:], payload(query)['messages'][-1:])

    def test_false_historical_claim_does_not_prime_standalone_tool_request(self):
        value = payload('音量设置成40%')
        value['messages'][1:3] = [
            {'role': 'user', 'content': '音量设置成50%'},
            {'role': 'assistant', 'content': '音量已设置为50%。当前音量为50%。'}]
        result = rewrite(value)
        self.assertEqual(result['messages'][1:], [value['messages'][-1]])
        self.assertNotIn('50%', json.dumps(result, ensure_ascii=False))

    def test_nonstandalone_and_negation_keep_general_path(self):
        for query in ['不要把音量设置成50%', '解释如何把音量设置成50%', '翻译：音量设置成50%',
                      '音量设置成50%，然后打开文件管理器', '将日志保存到桌面', '音量设置成101%',
                      '音量设置成-1%', '把音量调大一点', '请总结这页手册', '7乘8等于几？']:
            with self.subTest(query=query):
                value = rewrite(payload(query))
                self.assertEqual(len(value['tools']), len(NAMES))
                self.assertEqual(value['tool_choice'], 'auto')
                self.assertEqual(value['temperature'], 0.7)

    def test_missing_schema_cannot_be_invented(self):
        value = payload('音量设置成50%')
        value['tools'] = value['tools'][:4]
        self.assertEqual(len(rewrite(value)['tools']), 4)

    def test_malformed_tool_container_does_not_crash(self):
        for invalid in [None, 4, {}, 'shell']:
            value = payload('音量设置成50%')
            value['tools'] = invalid
            self.assertEqual(rewrite(value)['tools'], invalid)

    def test_explicit_none_or_forced_choice_preserved(self):
        for choice in ['none', 'required', {'type': 'function', 'function': {'name': 'shell'}}]:
            value = payload('音量设置成50%')
            value['tool_choice'] = choice
            self.assertEqual(rewrite(value)['tool_choice'], choice)
            self.assertEqual(len(rewrite(value)['tools']), len(NAMES))

    def test_tool_result_keeps_full_history_and_all_tools(self):
        value = payload('音量设置成50%')
        value['messages'].append({'role': 'tool', 'content': 'readback50', 'tool_call_id': 'test'})
        result = rewrite(value)
        self.assertEqual(result['messages'][1:], value['messages'][1:])
        self.assertEqual(len(result['tools']), len(NAMES))

    def test_multimodal_not_reclassified(self):
        value = payload('[IMAGE:abc] 音量设置成50%')
        self.assertEqual(len(rewrite(value)['tools']), len(NAMES))
        value['messages'][-1]['content'] = [{'type': 'text', 'text': '音量设置成50%'}]
        self.assertEqual(len(rewrite(value)['tools']), len(NAMES))


if __name__ == '__main__':
    unittest.main()
