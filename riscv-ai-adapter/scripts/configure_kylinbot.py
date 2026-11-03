#!/usr/bin/env python3
"""Configure KylinBot for the K1 Adapter competition profile."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import subprocess
import tempfile
import tomllib


DEFAULT_PROVIDER = "custom:http://127.0.0.1:18080/v1"
DEFAULT_BASE_URL = "http://127.0.0.1:18080/v1"
DEFAULT_GATEWAY_TRANSPORT = "uds"
COMPETITION_COMMANDS = (
    "git",
    "npm",
    "cargo",
    "ls",
    "cat",
    "grep",
    "find",
    "echo",
    "pwd",
    "wc",
    "head",
    "tail",
    "date",
    "df",
    "du",
    "uname",
    "uptime",
    "hostname",
    "python",
    "python3",
    "pip",
    "node",
    "free",
    "wpctl",
    "pcmanfm-qt",
    "lxqt-config",
    "xdg-open",
)


def _toml_value(value: str | int | bool | tuple[str, ...]) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    return json.dumps(value)


def _section_bounds(lines: list[str], section: str) -> tuple[int, int] | None:
    header = f"[{section}]"
    start = next(
        (index + 1 for index, line in enumerate(lines) if line.strip() == header),
        None,
    )
    if start is None:
        return None
    end = next(
        (
            index
            for index in range(start, len(lines))
            if lines[index].lstrip().startswith("[")
        ),
        len(lines),
    )
    return start, end


def _set_field(
    lines: list[str],
    section: str,
    key: str,
    value: str | int | bool | tuple[str, ...],
) -> None:
    bounds = _section_bounds(lines, section)
    if bounds is None:
        if lines and lines[-1].strip():
            lines.append("\n")
        lines.extend([f"[{section}]\n", f"{key} = {_toml_value(value)}\n"])
        return
    start, end = bounds
    for index in range(start, end):
        content = lines[index].lstrip()
        if content.startswith(f"{key} ") or content.startswith(f"{key}="):
            lines[index] = f"{key} = {_toml_value(value)}\n"
            return
    lines.insert(end, f"{key} = {_toml_value(value)}\n")


def configure_text(
    original: str,
    provider: str = DEFAULT_PROVIDER,
    base_url: str = DEFAULT_BASE_URL,
    model: str = "qwen-local",
    timeout_secs: int = 600,
    allowed_roots: tuple[str, ...] = (),
    allowed_commands: tuple[str, ...] = COMPETITION_COMMANDS,
    auto_approve_tools: tuple[str, ...] = ("shell",),
    gateway_transport: str = DEFAULT_GATEWAY_TRANSPORT,
    gateway_socket_path: str = "",
) -> str:
    lines = original.splitlines(keepends=True)
    if not lines:
        lines = ["schema_version = 2\n", "\n"]
    _set_field(lines, "providers", "fallback", provider)
    _set_field(lines, "providers.models.default", "name", provider)
    _set_field(lines, "providers.models.default", "base_url", base_url)
    _set_field(lines, "providers.models.default", "model", model)
    _set_field(lines, "providers.models.default", "timeout_secs", timeout_secs)

    provider_section = f'providers.models."{provider}"'
    _set_field(lines, provider_section, "base_url", base_url)
    _set_field(lines, provider_section, "model", model)
    _set_field(lines, provider_section, "timeout_secs", timeout_secs)

    # Small-K's native Agent client speaks HTTP/WebSocket over a local Unix
    # socket.  Using the same transport here avoids exposing the Gateway and
    # keeps the desktop client, voice client and KylinBot on one endpoint.
    _set_field(lines, "gateway", "transport", gateway_transport)
    if gateway_transport == "uds" and gateway_socket_path:
        _set_field(lines, "gateway", "socket_path", gateway_socket_path)

    # Keep the prompt and history inside the 8K llama.cpp context used on K1.
    _set_field(lines, "agent", "compact_context", True)
    _set_field(lines, "agent", "max_history_messages", 12)
    _set_field(lines, "agent", "max_context_tokens", 7000)
    _set_field(lines, "agent", "max_system_prompt_chars", 1500)
    _set_field(lines, "agent", "max_tool_result_chars", 8000)
    # Session history remains available; disable background LLM consolidation
    # so it cannot compete with an interactive turn on the single K1 backend.
    _set_field(lines, "memory", "auto_save", False)
    if allowed_roots:
        _set_field(lines, "autonomy", "allowed_roots", allowed_roots)
    if allowed_commands:
        _set_field(
            lines,
            "autonomy.security.rules.whitelist",
            "allowed_commands",
            allowed_commands,
        )
    if auto_approve_tools:
        _set_field(lines, "autonomy", "auto_approve", auto_approve_tools)
    result = "".join(lines)
    tomllib.loads(result)
    return result


def atomic_write(path: pathlib.Path, content: str) -> None:
    original_mode = path.stat().st_mode & 0o777
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        newline="",
        dir=path.parent,
        prefix=path.name + ".",
        delete=False,
    ) as handle:
        handle.write(content)
        temporary = pathlib.Path(handle.name)
    os.chmod(temporary, original_mode)
    os.replace(temporary, path)


def xdg_directory(name: str) -> str:
    try:
        result = subprocess.run(
            ["xdg-user-dir", name],
            check=True,
            capture_output=True,
            text=True,
            timeout=3,
        )
    except (OSError, subprocess.SubprocessError):
        return ""
    return result.stdout.strip()


def default_gateway_socket_path() -> str:
    runtime_dir = os.environ.get("XDG_RUNTIME_DIR", "").strip()
    if not runtime_dir:
        runtime_dir = f"/run/user/{os.getuid()}"
    # This is also the fallback used by kylin-aiassistant-lite.
    return str(pathlib.Path(runtime_dir) / "kylin-bot" / "gateway.sock")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--config",
        type=pathlib.Path,
        default=pathlib.Path.home() / ".kylinbot" / "config.toml",
    )
    parser.add_argument("--provider", default=DEFAULT_PROVIDER)
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    parser.add_argument("--model", default="qwen-local")
    parser.add_argument("--timeout-secs", type=int, default=600)
    parser.add_argument(
        "--gateway-transport",
        choices=("uds", "tcp"),
        default=DEFAULT_GATEWAY_TRANSPORT,
    )
    parser.add_argument(
        "--gateway-socket-path",
        default="",
        help="Unix socket path (default: XDG_RUNTIME_DIR/kylin-bot/gateway.sock)",
    )
    parser.add_argument("--restore", action="store_true")
    args = parser.parse_args()

    path = args.config.expanduser().resolve()
    backup = path.with_name(path.name + ".pre-rvai")
    if args.restore:
        if not backup.is_file():
            parser.error(f"backup does not exist: {backup}")
        shutil.copy2(backup, path)
        print(f"restored {path} from {backup}")
        return 0
    if args.timeout_secs <= 0:
        parser.error("--timeout-secs must be positive")
    original = path.read_text(encoding="utf-8")
    parsed = tomllib.loads(original)
    autonomy = parsed.get("autonomy", {})
    existing_roots = autonomy.get("allowed_roots", []) if isinstance(autonomy, dict) else []
    existing_auto_approve = (
        autonomy.get("auto_approve", []) if isinstance(autonomy, dict) else []
    )
    discovered_roots = [xdg_directory("DOCUMENTS"), xdg_directory("DESKTOP")]
    allowed_roots = tuple(
        dict.fromkeys(
            str(item)
            for item in [*existing_roots, *discovered_roots]
            if str(item).strip()
        )
    )
    auto_approve_tools = tuple(
        dict.fromkeys(
            str(item)
            for item in [*existing_auto_approve, "shell"]
            if str(item).strip()
        )
    )
    gateway_socket_path = args.gateway_socket_path.strip()
    if args.gateway_transport == "uds" and not gateway_socket_path:
        gateway_socket_path = default_gateway_socket_path()
    updated = configure_text(
        original,
        provider=args.provider,
        base_url=args.base_url,
        model=args.model,
        timeout_secs=args.timeout_secs,
        allowed_roots=allowed_roots,
        auto_approve_tools=auto_approve_tools,
        gateway_transport=args.gateway_transport,
        gateway_socket_path=gateway_socket_path,
    )
    if not backup.exists():
        shutil.copy2(path, backup)
    if updated == original:
        print(f"already configured: {path}")
        return 0
    atomic_write(path, updated)
    print(f"configured {path}; original backup: {backup}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
