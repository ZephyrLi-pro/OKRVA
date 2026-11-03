from __future__ import annotations

import importlib.util
import pathlib
import tomllib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "configure_kylinbot", ROOT / "scripts" / "configure_kylinbot.py"
)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ConfigureKylinBotTest(unittest.TestCase):
    def test_adds_named_custom_provider_and_keeps_other_sections(self) -> None:
        source = '''\
schema_version = 2

[providers]
fallback = "ollama"

[providers.models.default]
name = "ollama"
base_url = "http://127.0.0.1:11434"
model = "qwen2.5:7b"
timeout_secs = 120

[channels.voice_wake]
enabled = true
'''
        updated = MODULE.configure_text(
            source,
            allowed_roots=("/home/test/Documents", "/home/test/Desktop"),
            gateway_socket_path="/run/user/1000/kylin-bot/gateway.sock",
        )
        parsed = tomllib.loads(updated)
        provider = "custom:http://127.0.0.1:18080/v1"
        self.assertEqual(parsed["providers"]["fallback"], provider)
        self.assertEqual(parsed["providers"]["models"]["default"]["name"], provider)
        self.assertEqual(
            parsed["providers"]["models"][provider]["model"], "qwen-local"
        )
        self.assertTrue(parsed["channels"]["voice_wake"]["enabled"])
        self.assertEqual(parsed["agent"]["max_context_tokens"], 7000)
        self.assertEqual(parsed["agent"]["max_system_prompt_chars"], 1500)
        self.assertEqual(
            parsed["autonomy"]["allowed_roots"],
            ["/home/test/Documents", "/home/test/Desktop"],
        )
        self.assertIn(
            "wpctl",
            parsed["autonomy"]["security"]["rules"]["whitelist"][
                "allowed_commands"
            ],
        )
        self.assertIn("shell", parsed["autonomy"]["auto_approve"])
        self.assertEqual(parsed["gateway"]["transport"], "uds")
        self.assertEqual(
            parsed["gateway"]["socket_path"],
            "/run/user/1000/kylin-bot/gateway.sock",
        )
        self.assertFalse(parsed["memory"]["auto_save"])

    def test_is_idempotent(self) -> None:
        source = "schema_version = 2\n"
        first = MODULE.configure_text(source)
        self.assertEqual(MODULE.configure_text(first), first)


if __name__ == "__main__":
    unittest.main()
