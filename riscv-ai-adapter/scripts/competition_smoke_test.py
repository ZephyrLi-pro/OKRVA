#!/usr/bin/env python3
"""Exercise the minimum K1 competition path without third-party packages."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
import time
import urllib.error
import urllib.request
import uuid


def request_json(
    url: str,
    *,
    data: bytes | None = None,
    content_type: str = "application/json",
    timeout: float = 30,
) -> tuple[int, dict[str, object]]:
    request = urllib.request.Request(url, data=data)
    if data is not None:
        request.add_header("Content-Type", content_type)
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return response.status, json.loads(response.read())
    except urllib.error.HTTPError as error:
        try:
            body = json.loads(error.read())
        except json.JSONDecodeError:
            body = {"error": {"message": str(error)}}
        return error.code, body


def wait_ready(base_url: str, wait_seconds: float) -> dict[str, object]:
    deadline = time.monotonic() + wait_seconds
    last_status = 0
    last_body: dict[str, object] = {}
    while time.monotonic() < deadline:
        try:
            last_status, last_body = request_json(base_url + "/health", timeout=5)
        except OSError as error:
            last_status, last_body = 0, {"error": {"message": str(error)}}
        if last_status == 200 and last_body.get("status") == "ready":
            return last_body
        time.sleep(1)
    raise RuntimeError(
        f"adapter did not become ready in {wait_seconds:g}s: "
        f"HTTP {last_status} {last_body}"
    )


def check_tool_call(base_url: str) -> dict[str, object]:
    payload = {
        "model": "qwen-local",
        "messages": [
            {
                "role": "system",
                "content": "你是系统助手。需要执行操作时必须调用工具，不要用文字代替。",
            },
            {"role": "user", "content": "请调用工具执行 pwd，不要自己猜测结果。"},
        ],
        "tools": [
            {
                "type": "function",
                "function": {
                    "name": "shell",
                    "description": "Execute a shell command in the workspace directory",
                    "parameters": {
                        "type": "object",
                        "properties": {
                            "command": {"type": "string"},
                            "approved": {"type": "boolean", "default": False},
                        },
                        "required": ["command"],
                    },
                },
            }
        ],
        "tool_choice": "auto",
        "temperature": 0,
        "max_tokens": 128,
    }
    status, body = request_json(
        base_url + "/v1/chat/completions",
        data=json.dumps(payload, ensure_ascii=False).encode(),
        timeout=120,
    )
    if status != 200:
        raise RuntimeError(f"chat request failed: HTTP {status} {body}")
    choices = body.get("choices")
    if not isinstance(choices, list) or not choices:
        raise RuntimeError(f"chat response has no choices: {body}")
    message = choices[0].get("message", {})
    calls = message.get("tool_calls") if isinstance(message, dict) else None
    if not isinstance(calls, list) or not calls:
        raise RuntimeError(f"model did not produce a tool call: {body}")
    function = calls[0].get("function", {})
    if not isinstance(function, dict) or function.get("name") != "shell":
        raise RuntimeError(f"unexpected tool call: {calls[0]}")
    arguments = json.loads(str(function.get("arguments", "{}")))
    if str(arguments.get("command", "")).strip() != "pwd":
        raise RuntimeError(f"unexpected tool arguments: {arguments}")
    return body


def multipart_audio(path: pathlib.Path) -> tuple[bytes, str]:
    boundary = "----rvai-" + uuid.uuid4().hex
    audio = path.read_bytes()
    parts = [
        f"--{boundary}\r\n".encode(),
        (
            'Content-Disposition: form-data; name="file"; '
            f'filename="{path.name}"\r\n'
        ).encode(),
        b"Content-Type: audio/wav\r\n\r\n",
        audio,
        b"\r\n",
        f"--{boundary}\r\n".encode(),
        b'Content-Disposition: form-data; name="model"\r\n\r\n',
        b"paraformer-zh-small\r\n",
        f"--{boundary}--\r\n".encode(),
    ]
    return b"".join(parts), f"multipart/form-data; boundary={boundary}"


def check_asr(base_url: str, audio_path: pathlib.Path) -> dict[str, object]:
    body, content_type = multipart_audio(audio_path)
    status, result = request_json(
        base_url + "/v1/audio/transcriptions",
        data=body,
        content_type=content_type,
        timeout=120,
    )
    if status != 200 or not str(result.get("text", "")).strip():
        raise RuntimeError(f"ASR request failed: HTTP {status} {result}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:18080")
    parser.add_argument("--wait-seconds", type=float, default=60)
    parser.add_argument("--audio", type=pathlib.Path)
    args = parser.parse_args()
    base_url = args.url.rstrip("/")

    try:
        health = wait_ready(base_url, args.wait_seconds)
        print(f"[PASS] adapter ready: {health.get('architecture', 'unknown')}")
        result = check_tool_call(base_url)
        model = result.get("model", "unknown")
        print(f"[PASS] local LLM tool calling: {model}")
        if args.audio:
            transcript = check_asr(base_url, args.audio)
            print(f"[PASS] local ASR: {transcript.get('text')}")
        else:
            print("[SKIP] local ASR request (pass --audio FILE.wav to test)")
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
