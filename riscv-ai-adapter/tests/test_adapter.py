from __future__ import annotations

import json
import pathlib
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from dataclasses import replace
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from riscv_ai_adapter.backends import rewrite_chat_request  # noqa: E402
from riscv_ai_adapter.config import load_config  # noqa: E402
from riscv_ai_adapter.server import make_server  # noqa: E402


class FakeBackendHandler(BaseHTTPRequestHandler):
    def log_message(self, _fmt: str, *_args: object) -> None:
        pass

    def do_GET(self) -> None:  # noqa: N802
        body = b'{"status":"ready"}'
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self) -> None:  # noqa: N802
        length = int(self.headers.get("Content-Length", "0"))
        request_body = self.rfile.read(length)
        if self.path.endswith("chat/completions"):
            request_json = json.loads(request_body)
            body = json.dumps(
                {
                    "model": request_json["model"],
                    "messages": request_json.get("messages", []),
                    "tools": request_json.get("tools", []),
                    "choices": [{"message": {"role": "assistant", "content": "ok"}}],
                }
            ).encode()
        else:
            body = b'{"text":"test speech"}'
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class AdapterTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.backend = ThreadingHTTPServer(("127.0.0.1", 0), FakeBackendHandler)
        cls.backend_thread = threading.Thread(target=cls.backend.serve_forever, daemon=True)
        cls.backend_thread.start()
        backend_port = cls.backend.server_address[1]

        config_text = f'''\
[server]
host = "127.0.0.1"
port = 18080

[resource_groups]
heavy = 1

[[backends]]
name = "fake-llm"
capability = "chat"
required = true
url = "http://127.0.0.1:{backend_port}/v1/chat/completions"
health_url = "http://127.0.0.1:{backend_port}/health"
models = ["qwen-local"]
upstream_model = "backend-model"
allowed_tools = ["shell"]
max_system_prompt_chars = 20
resource_group = "heavy"

[[backends]]
name = "fake-asr"
capability = "transcription"
required = true
url = "http://127.0.0.1:{backend_port}/v1/audio/transcriptions"
health_url = "http://127.0.0.1:{backend_port}/health"
models = ["paraformer"]
'''
        cls.temp = tempfile.TemporaryDirectory()
        cls.config_path = pathlib.Path(cls.temp.name) / "config.toml"
        cls.config_path.write_text(config_text, encoding="utf-8")
        cls.config = load_config(cls.config_path)
        cls.config = replace(
            cls.config,
            server=replace(cls.config.server, port=0),
        )
        cls.adapter = make_server(cls.config)
        cls.adapter_thread = threading.Thread(target=cls.adapter.serve_forever, daemon=True)
        cls.adapter_thread.start()
        cls.base_url = f"http://127.0.0.1:{cls.adapter.server_address[1]}"

    @classmethod
    def tearDownClass(cls) -> None:
        cls.adapter.shutdown()
        cls.adapter.server_close()
        cls.backend.shutdown()
        cls.backend.server_close()
        cls.temp.cleanup()

    def get_json(self, path: str) -> tuple[int, dict[str, object]]:
        try:
            with urllib.request.urlopen(self.base_url + path, timeout=3) as response:
                return response.status, json.loads(response.read())
        except urllib.error.HTTPError as error:
            return error.code, json.loads(error.read())

    def test_health_and_kserve_style_readiness(self) -> None:
        status, body = self.get_json("/health")
        self.assertEqual(status, 200)
        self.assertEqual(body["status"], "ready")
        status, body = self.get_json("/v2/health/ready")
        self.assertEqual(status, 200)
        self.assertTrue(body["ready"])

    def test_models_exposes_aliases(self) -> None:
        status, body = self.get_json("/v1/models")
        self.assertEqual(status, 200)
        model_ids = {item["id"] for item in body["data"]}
        self.assertEqual(model_ids, {"qwen-local", "paraformer"})

    def test_system_and_scheduler_status(self) -> None:
        status, system = self.get_json("/v1/system/info")
        self.assertEqual(status, 200)
        self.assertGreater(system["cpu_count"], 0)
        status, scheduler = self.get_json("/v1/scheduler")
        self.assertEqual(status, 200)
        self.assertIn("group:heavy", scheduler)

    def test_chat_routes_and_rewrites_model(self) -> None:
        request = urllib.request.Request(
            self.base_url + "/v1/chat/completions",
            data=json.dumps(
                {
                    "model": "qwen-local",
                    "messages": [
                        {"role": "system", "content": "x" * 100},
                        {"role": "user", "content": "hello"},
                    ],
                    "tools": [
                        {"type": "function", "function": {"name": "shell"}},
                        {"type": "function", "function": {"name": "web_search"}},
                    ],
                }
            ).encode(),
            method="POST",
            headers={"Content-Type": "application/json"},
        )
        with urllib.request.urlopen(request, timeout=3) as response:
            body = json.loads(response.read())
            self.assertEqual(response.headers["X-RVAI-Backend"], "fake-llm")
        self.assertEqual(body["model"], "backend-model")
        self.assertEqual(body["tools"], [{"type": "function", "function": {"name": "shell"}}])
        self.assertEqual(len(body["messages"][0]["content"]), 20)
        self.assertEqual(body["messages"][1]["content"], "hello")

    def test_system_prompt_override(self) -> None:
        source = json.dumps(
            {
                "model": "alias",
                "messages": [
                    {"role": "system", "content": "large upstream prompt"},
                    {"role": "user", "content": "hello"},
                ],
            }
        ).encode()
        rewritten = json.loads(
            rewrite_chat_request(
                source,
                "backend-model",
                max_system_prompt_chars=100,
                system_prompt_override="compact planner prompt",
            )
        )
        self.assertEqual(rewritten["messages"][0]["content"], "compact planner prompt")
        self.assertEqual(rewritten["messages"][1]["content"], "hello")

    def test_transcription_preserves_multipart_body(self) -> None:
        multipart = b"--x\r\nContent-Disposition: form-data; name=file\r\n\r\nWAV\r\n--x--\r\n"
        request = urllib.request.Request(
            self.base_url + "/v1/audio/transcriptions",
            data=multipart,
            method="POST",
            headers={"Content-Type": "multipart/form-data; boundary=x"},
        )
        with urllib.request.urlopen(request, timeout=3) as response:
            body = json.loads(response.read())
            self.assertEqual(response.headers["X-RVAI-Backend"], "fake-asr")
        self.assertEqual(body["text"], "test speech")

    def test_unknown_route_is_404(self) -> None:
        status, _ = self.get_json("/missing")
        self.assertEqual(status, 404)


class SimpleConversationTest(unittest.TestCase):
    def rewrite(self, content: object, **extra: object) -> dict:
        value = {
            "model": "local", "messages": [{"role": "user", "content": content}],
            "tools": [{"type": "function", "function": {"name": "shell"}}],
            "max_tokens": 1024,
        }
        value.update(extra)
        return json.loads(rewrite_chat_request(
            json.dumps(value).encode(), "backend", ("shell",), 1500,
            "你是本地系统助手。系统操作必须调用工具。",
        ))

    def test_identity_is_bounded_chat_without_tools(self) -> None:
        result = self.rewrite("你是谁？", parallel_tool_calls=True)
        self.assertNotIn("tools", result)
        self.assertNotIn("parallel_tool_calls", result)
        self.assertEqual(result["max_tokens"], 80)
        self.assertEqual(result["messages"][-1]["content"], "你是谁？")

    def test_small_k_prompt_and_timestamp_are_unwrapped(self) -> None:
        result = self.rewrite("2026-10-09 15:43:05 +08:00 请仅使用纯文本回答。用户问题：你是谁。")
        self.assertNotIn("tools", result)
        self.assertEqual(result["messages"][-1]["content"], "你是谁。")

    def test_actual_kylinbot_bracketed_timestamp_is_unwrapped(self) -> None:
        result = self.rewrite("[2026-10-10 11:36:45 +08:00] 你是谁？")
        self.assertNotIn("tools", result)
        self.assertEqual(result["messages"][-1]["content"], "你是谁？")

    def test_timestamp_in_general_input_is_preserved(self) -> None:
        content = "[2026-10-10 11:36:45 +08:00] 帮我解释这条日志：Permission denied"
        result = self.rewrite(content)
        self.assertEqual(result["messages"][-1]["content"], content)
        self.assertIn("tools", result)

    def test_fragment_asks_for_clarification_not_a_tool(self) -> None:
        result = self.rewrite("我。")
        self.assertNotIn("tools", result)
        self.assertIn("输入不完整", result["messages"][0]["content"])

    def test_system_and_compound_requests_keep_tools(self) -> None:
        for text in ("打开系统设置", "你好，然后将音量调到50%", "查找最近修改的三份文件", "这一页主要讲什么"):
            with self.subTest(text=text):
                result = self.rewrite(text)
                self.assertEqual(len(result["tools"]), 1)
                self.assertEqual(result["max_tokens"], 1024)

    def test_forced_tool_call_is_not_overridden(self) -> None:
        for choice in ("required", {"type": "function", "function": {"name": "shell"}}):
            result = self.rewrite("你好", tool_choice=choice)
            self.assertIn("tools", result)
            self.assertEqual(result["tool_choice"], choice)

    def test_image_or_tool_result_does_not_take_chat_shortcut(self) -> None:
        for content in ("你好 [IMAGE:data:image/png;base64,abc]", [{"type": "text", "text": "你好"}]):
            self.assertIn("tools", self.rewrite(content))
        self.assertIn("tools", self.rewrite("", messages=[{"role": "tool", "content": "你好"}]))

    def test_smaller_limit_is_preserved_and_invalid_limit_does_not_crash(self) -> None:
        self.assertEqual(self.rewrite("你好", max_tokens=20)["max_tokens"], 20)
        self.assertEqual(self.rewrite("你好", max_tokens="bad")["max_tokens"], 80)

    def test_model_only_rewrite_does_not_change_chat_policy(self) -> None:
        value = {"messages": [{"role": "user", "content": "你好"}], "tools": ["tool"]}
        result = json.loads(rewrite_chat_request(json.dumps(value).encode(), "backend"))
        self.assertEqual(result["tools"], ["tool"])


class ConfigValidationTest(unittest.TestCase):
    def test_ollama_tool_call_compat_requires_a_boolean(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "config.toml"
            prefix = '''[[backends]]
name = "chat"
capability = "chat"
url = "http://127.0.0.1:9999/v1/chat/completions"
'''
            for value in ('"true"', "1"):
                path.write_text(prefix + f"ollama_tool_call_compat = {value}\n", encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "must be boolean"):
                    load_config(path)
            path.write_text(prefix + "ollama_tool_call_compat = true\n", encoding="utf-8")
            self.assertTrue(load_config(path).backends[0].ollama_tool_call_compat)

    def test_remote_bind_requires_explicit_opt_in(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "config.toml"
            path.write_text(
                '''\
[server]
host = "0.0.0.0"

[[backends]]
name = "chat"
capability = "chat"
url = "http://127.0.0.1:9999/v1/chat/completions"
''',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "non-loopback"):
                load_config(path)


class OllamaCompatibilityTest(unittest.TestCase):
    def test_empty_assistant_tool_content_is_null_and_history_is_preserved(self) -> None:
        call = {"id": "call_fixture", "type": "function", "function": {
            "name": "file_read", "arguments": '{"path":"manual.txt"}'}}
        value = {"messages": [
            {"role": "user", "content": "read manual.txt"},
            {"role": "assistant", "content": "", "tool_calls": [call]},
            {"role": "tool", "tool_call_id": "call_fixture", "content": "light: green"},
        ]}
        body = json.dumps(value).encode()
        result = json.loads(rewrite_chat_request(body, "", ollama_tool_call_compat=True))
        expected = json.loads(body)
        expected["messages"][1]["content"] = None
        self.assertEqual(result, expected)
        self.assertEqual(rewrite_chat_request(body, ""), body)

    def test_visible_assistant_text_and_non_tool_messages_are_untouched(self) -> None:
        value = {"messages": [
            {"role": "assistant", "content": "visible text", "tool_calls": [{}]},
            {"role": "assistant", "content": "", "tool_calls": []},
            {"role": "user", "content": ""},
        ]}
        result = json.loads(rewrite_chat_request(
            json.dumps(value).encode(), "", ollama_tool_call_compat=True))
        self.assertEqual(result, value)


if __name__ == "__main__":
    unittest.main()
