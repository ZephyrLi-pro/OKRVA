#!/usr/bin/env python3
"""Bounded desktop-volume MCP server; independent of the inference Adapter.

JSON-RPC is newline-delimited for KylinBot 0.7.4. No shell, arbitrary executable,
model-supplied environment, file access, radio control, or privilege escalation.
"""
from __future__ import annotations

from decimal import Decimal, ROUND_HALF_UP
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys

WPCTL = '/usr/bin/wpctl'
SINK = '@DEFAULT_AUDIO_SINK@'
MAX_LINE = 65536
VOLUME_PATTERN = re.compile(r'^Volume:\s+([0-9]+(?:\.[0-9]+)?)(?:\s+(\[MUTED\]))?\s*$')
SCHEMA = [
    {'name': 'set_volume', 'description': '设置系统播放音量百分比 (0-100)，执行后回读确认；不改变静音状态。',
     'inputSchema': {'type': 'object', 'properties': {
         'percent': {'type': 'integer', 'minimum': 0, 'maximum': 100,
                     'description': '目标音量百分比 0-100'}},
         'required': ['percent'], 'additionalProperties': False}},
    {'name': 'get_volume', 'description': '查询系统当前播放音量百分比及静音状态，不做修改。',
     'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False}},
]


class ToolError(Exception):
    def __init__(self, message: str, code: int = -32010):
        super().__init__(message)
        self.code = code


def run_wpctl(arguments: list[str]) -> str:
    runtime = Path('/run/user') / str(os.getuid())
    try:
        info = runtime.lstat()
        if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid():
            raise ToolError('桌面音频运行目录无效，未执行音量操作。')
        # Do not inherit LD_PRELOAD, PYTHONPATH, shell startup or arbitrary PATH.
        environment = {'PATH': '/usr/bin:/bin', 'LANG': 'C.UTF-8',
                       'LC_ALL': 'C.UTF-8', 'XDG_RUNTIME_DIR': str(runtime)}
        result = subprocess.run([WPCTL, *arguments], shell=False, capture_output=True,
                                text=True, encoding='utf-8', errors='replace',
                                timeout=4, check=False, env=environment)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ToolError('音频命令不可用或超时；未确认操作成功，请重新查询实际音量。') from error
    if result.returncode != 0:
        detail = (result.stderr or result.stdout or 'no diagnostic').strip()[:400]
        raise ToolError(f'音频命令失败（退出码{result.returncode}），未确认操作成功：{detail}')
    return result.stdout.strip()


def parse_volume(text: str) -> tuple[int, bool]:
    match = VOLUME_PATTERN.fullmatch(text)
    if not match:
        raise ToolError('无法识别实际音量读数，未确认操作成功。')
    percent = int((Decimal(match.group(1)) * 100).quantize(Decimal('1'), rounding=ROUND_HALF_UP))
    return percent, bool(match.group(2))


class VolumeTools:
    def __init__(self, runner=run_wpctl):
        self.runner = runner

    def call(self, name: str, arguments: object) -> str:
        if not isinstance(arguments, dict):
            raise ToolError('工具参数必须是JSON对象。', -32602)
        if name == 'get_volume':
            if arguments:
                raise ToolError('get_volume不接受参数。', -32602)
            percent, muted = parse_volume(self.runner(['get-volume', SINK]))
            return f'系统当前音量为{percent}%，' + ('已静音。' if muted else '未静音。')
        if name != 'set_volume':
            raise ToolError('未开放此工具。', -32602)
        if set(arguments) != {'percent'}:
            raise ToolError('set_volume仅接受percent参数。', -32602)
        percent = arguments['percent']
        if type(percent) is not int or not 0 <= percent <= 100:
            raise ToolError('percent必须是0到100的整数；未执行设置。', -32602)
        # Never combine setter and getter with ';': each exit status matters.
        self.runner(['set-volume', SINK, f'{percent}%'])
        actual, muted = parse_volume(self.runner(['get-volume', SINK]))
        if actual != percent:
            raise ToolError(f'音量验证失败：目标{percent}%，实际{actual}%；不能报告设置成功。')
        return (f'系统音量已设置为{actual}%，已回读确认。'
                + ('当前仍处于静音状态。' if muted else ''))


def rpc_error(request_id, code: int, message: str):
    return {'jsonrpc': '2.0', 'id': request_id, 'error': {'code': code, 'message': message}}


def handle(message: object, tools: VolumeTools):
    if not isinstance(message, dict) or message.get('jsonrpc') != '2.0':
        return rpc_error(None, -32600, 'Invalid JSON-RPC request')
    request_id = message.get('id')
    method = message.get('method')
    if not isinstance(method, str):
        return rpc_error(request_id, -32600, 'Invalid JSON-RPC method')
    # Notifications, including a notification named tools/call, never mutate.
    if request_id is None:
        return None
    if type(request_id) not in (str, int):
        return rpc_error(None, -32600, 'Invalid JSON-RPC id')
    params = message.get('params', {})
    if not isinstance(params, dict):
        return rpc_error(request_id, -32602, 'Parameters must be an object')
    try:
        if method == 'initialize':
            result = {'protocolVersion': '2024-11-05',
                      'capabilities': {'tools': {'listChanged': False}},
                      'serverInfo': {'name': 'bianbu-system-tools', 'version': '0.1.0'}}
        elif method == 'ping':
            result = {}
        elif method == 'tools/list':
            result = {'tools': SCHEMA}
        elif method == 'tools/call':
            if set(params) - {'name', 'arguments'}:
                raise ToolError('未知调用字段。', -32602)
            text = tools.call(params.get('name'), params.get('arguments', {}))
            result = {'content': [{'type': 'text', 'text': text}], 'isError': False}
        else:
            return rpc_error(request_id, -32601, 'Method not found')
    except ToolError as error:
        # KylinBot 0.7.4 ignores result.isError, but propagates JSON-RPC errors.
        # Explicit application errors prevent failed actions being marked success.
        return rpc_error(request_id, error.code, str(error))
    return {'jsonrpc': '2.0', 'id': request_id, 'result': result}


def serve(source, target, tools=None):
    tools = tools or VolumeTools()
    while True:
        line = source.readline(MAX_LINE + 1)
        if not line:
            return
        if len(line) > MAX_LINE:
            # Bound input memory; discard the rest of this oversized request.
            while line and not line.endswith('\n'):
                line = source.readline(MAX_LINE + 1)
            response = rpc_error(None, -32600, 'Request exceeds input limit')
        elif not line.strip():
            continue
        else:
            try:
                response = handle(json.loads(line), tools)
            except json.JSONDecodeError:
                response = rpc_error(None, -32700, 'Invalid JSON')
            except Exception:
                response = rpc_error(None, -32603, 'Internal error; action success not confirmed')
        if response is not None:
            target.write(json.dumps(response, ensure_ascii=False) + '\n')
            target.flush()


if __name__ == '__main__':
    serve(sys.stdin, sys.stdout)
