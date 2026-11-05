import contextlib
import io
import json
import threading
import unittest
import urllib.request
from unittest.mock import patch

from riscv_ai_adapter.backends import (
    _SMALL_K_VOICE_FORMAT_PREFIX as PREFIX, UpstreamResponse,
    standalone_conversation_request,
)
from riscv_ai_adapter.reference_context import HEADER, QUESTION
from riscv_ai_adapter.config import AdapterConfig, BackendConfig, ServerConfig
from riscv_ai_adapter.server import make_server


def envelope(question):
    page = json.dumps({'kind': 'reading_page', 'title': 'unrelated slide', 'page': 13,
                       'text': 'UNRELATED_PAGE_BODY ' * 80})
    return PREFIX + HEADER + json.dumps({'reference_text': page}) + QUESTION + question


class StandaloneConversationTests(unittest.TestCase):
    def body(self, text, **extra):
        value = dict(model='alias', stream=True, max_tokens=1024,
                     messages=[{'role': 'user', 'content': 'OLD_HISTORY ' * 100},
                               {'role': 'assistant', 'content': 'previous answer'},
                               {'role': 'user', 'content': text}],
                     tools=[{'type': 'function', 'function': {'name': 'shell'}}])
        value.update(extra)
        return json.dumps(value).encode()

    def normalize(self, body):
        return standalone_conversation_request(body, 'actual', ('shell',), 1500,
            '你是KylinBot，本地系统助手。系统操作必须调用工具。')

    def test_live_reader_and_old_history_do_not_inflate_identity_chat(self):
        for content in (PREFIX + '你是谁？', envelope('你是谁？')):
            body = self.body(content)
            before = bytes(body)
            result = json.loads(self.normalize(body))
            self.assertEqual(result['model'], 'actual')
            self.assertEqual(len(result['messages']), 2)
            self.assertEqual(result['messages'][-1]['content'], '你是谁？')
            self.assertEqual(result['max_tokens'], 80)
            self.assertTrue(result['stream'])
            self.assertNotIn('tools', result)
            self.assertNotIn('OLD_HISTORY', json.dumps(result))
            self.assertNotIn('UNRELATED_PAGE_BODY', json.dumps(result))
            self.assertEqual(body, before)

    def test_actions_references_compound_and_quoted_questions_keep_their_route(self):
        for text in ('把音量设为30%', '你是谁，然后打开设置', envelope('这一页主要讲了什么？'),
                     envelope('解释日志的原因'), '翻译：你是谁？',
                     '打开设置。用户问题：你是谁', '[IMAGE:abc]你是谁',
                     [{'type': 'text', 'text': '你是谁'}]):
            with self.subTest(text=text):
                self.assertIsNone(self.normalize(self.body(text)))
        for tool_choice in ('required', {'type': 'function', 'function': {'name': 'shell'}}):
            self.assertIsNone(self.normalize(self.body('你是谁', tool_choice=tool_choice)))
        self.assertIsNone(self.normalize(self.body('你是谁', messages=[{'role': 'tool', 'content': '{}'}])))

    def test_http_identity_skips_planner_but_still_calls_real_generation_backend(self):
        sent = []
        backend = BackendConfig('candidate', 'chat', 'http://127.0.0.1:1',
            upstream_model='actual', semantic_system_planner=True, grounded_system_planner=True,
            allowed_tools=('shell',), max_system_prompt_chars=1500,
            system_prompt_override='你是KylinBot，本地系统助手。系统操作必须调用工具。')
        server = make_server(AdapterConfig(ServerConfig(port=0), (backend,), {}))
        @contextlib.contextmanager
        def upstream(config, body, *args, **kwargs):
            sent.append(json.loads(body))
            yield UpstreamResponse(200, {'Content-Type': 'application/json'},
                io.BytesIO(b'{"choices":[{"message":{"content":"model answer"}}]}'))
        thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
        try:
            with (patch.object(server.grounded_planner, 'handle', side_effect=AssertionError('Unnecessary plan')),
                  patch.object(server.registry, 'open', upstream)):
                request = urllib.request.Request(f'http://127.0.0.1:{server.server_port}/v1/chat/completions',
                    self.body(envelope('你是谁？')), headers={'Content-Type': 'application/json'})
                with urllib.request.urlopen(request, timeout=3) as response:
                    self.assertEqual(json.loads(response.read())['choices'][0]['message']['content'], 'model answer')
                self.assertEqual(len(sent), 1)
                self.assertEqual(len(sent[0]['messages']), 2)
                self.assertEqual(sent[0]['max_tokens'], 80)
                self.assertNotIn('tools', sent[0])
        finally:
            server.shutdown(); server.server_close(); thread.join(timeout=2)


if __name__ == '__main__':
    unittest.main()
