from __future__ import annotations

import ipaddress
import math
import os
import pathlib
import re
import tomllib
from dataclasses import dataclass, field
from typing import Any


_ENV_PATTERN = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}")


def expand_env(value: str) -> str:
    """Expand ${NAME} without invoking a shell."""

    def replace(match: re.Match[str]) -> str:
        name = match.group(1)
        if name not in os.environ:
            raise ValueError(f"environment variable {name!r} is not set")
        return os.environ[name]

    return _ENV_PATTERN.sub(replace, value)


@dataclass(frozen=True)
class ServerConfig:
    host: str = "127.0.0.1"
    port: int = 18080
    request_limit_bytes: int = 32 * 1024 * 1024
    token_env: str = ""
    allow_remote: bool = False


@dataclass(frozen=True)
class BackendConfig:
    name: str
    capability: str
    url: str
    enabled: bool = True
    required: bool = False
    health_url: str = ""
    models: tuple[str, ...] = ()
    upstream_model: str = ""
    allowed_tools: tuple[str, ...] = ()
    max_system_prompt_chars: int = 0
    system_prompt_override: str = ""
    ollama_tool_call_compat: bool = False
    semantic_system_planner: bool = False
    grounded_system_planner: bool = False
    document_tasks: bool = False
    system_planner_backend: str = ''
    reading_summary_backend: str = ''
    on_demand: bool = False
    idle_timeout_seconds: float = 0
    api_key_env: str = ""
    timeout_seconds: float = 300.0
    health_timeout_seconds: float = 2.0
    max_concurrency: int = 1
    acquire_timeout_seconds: float = 1.0
    priority: int = 50
    resource_group: str = ""
    managed: bool = False
    command: tuple[str, ...] = ()
    working_directory: str = ""
    environment: dict[str, str] = field(default_factory=dict)
    restart_on_failure: bool = True
    restart_backoff_seconds: float = 2.0
    max_restarts: int = 5
    exclusive_with: tuple[str, ...] = ()


@dataclass(frozen=True)
class AdapterConfig:
    server: ServerConfig
    backends: tuple[BackendConfig, ...]
    resource_groups: dict[str, int]


def _require_table(value: Any, name: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f"{name} must be a TOML table")
    return value


def _as_string_tuple(value: Any, field_name: str) -> tuple[str, ...]:
    if value is None:
        return ()
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise ValueError(f"{field_name} must be an array of strings")
    return tuple(value)


def load_config(path: str | os.PathLike[str]) -> AdapterConfig:
    config_path = pathlib.Path(path).expanduser().resolve()
    with config_path.open("rb") as handle:
        raw = tomllib.load(handle)

    server_raw = _require_table(raw.get("server", {}), "server")
    server = ServerConfig(
        host=str(server_raw.get("host", "127.0.0.1")),
        port=int(server_raw.get("port", 18080)),
        request_limit_bytes=int(
            server_raw.get("request_limit_bytes", 32 * 1024 * 1024)
        ),
        token_env=str(server_raw.get("token_env", "")),
        allow_remote=bool(server_raw.get("allow_remote", False)),
    )

    if not 1 <= server.port <= 65535:
        raise ValueError("server.port must be between 1 and 65535")
    if server.request_limit_bytes <= 0:
        raise ValueError("server.request_limit_bytes must be positive")
    try:
        address = ipaddress.ip_address(server.host)
        is_loopback = address.is_loopback
    except ValueError:
        is_loopback = server.host == "localhost"
    if not is_loopback and not server.allow_remote:
        raise ValueError(
            "refusing a non-loopback listen address unless server.allow_remote=true"
        )
    if server.allow_remote and not server.token_env:
        raise ValueError("server.token_env is required when remote access is enabled")

    groups_raw = _require_table(raw.get("resource_groups", {}), "resource_groups")
    resource_groups: dict[str, int] = {}
    for name, limit in groups_raw.items():
        parsed_limit = int(limit)
        if parsed_limit <= 0:
            raise ValueError(f"resource group {name!r} must have a positive limit")
        resource_groups[str(name)] = parsed_limit

    backends_raw = raw.get("backends", [])
    if not isinstance(backends_raw, list):
        raise ValueError("backends must be an array of tables")

    allowed_capabilities = {"chat", "transcription", "speech", "ocr", "embedding"}
    names: set[str] = set()
    backends: list[BackendConfig] = []
    for index, item in enumerate(backends_raw):
        table = _require_table(item, f"backends[{index}]")
        name = str(table.get("name", "")).strip()
        capability = str(table.get("capability", "")).strip()
        url = expand_env(str(table.get("url", "")).strip())
        if not name:
            raise ValueError(f"backends[{index}].name must not be empty")
        if name in names:
            raise ValueError(f"duplicate backend name {name!r}")
        names.add(name)
        if capability not in allowed_capabilities:
            raise ValueError(
                f"backend {name!r} has unsupported capability {capability!r}"
            )
        if not url.startswith(("http://", "https://")):
            raise ValueError(f"backend {name!r} url must use http or https")

        resource_group = str(table.get("resource_group", "")).strip()
        if resource_group and resource_group not in resource_groups:
            raise ValueError(
                f"backend {name!r} references unknown resource group {resource_group!r}"
            )

        command = _as_string_tuple(table.get("command"), f"backend {name}.command")
        managed = bool(table.get("managed", False))
        if managed and not command:
            raise ValueError(f"managed backend {name!r} requires a command")

        environment_raw = _require_table(
            table.get("environment", {}), f"backend {name}.environment"
        )
        environment = {
            str(key): expand_env(str(value)) for key, value in environment_raw.items()
        }
        tool_call_compat = table.get("ollama_tool_call_compat", False)
        if type(tool_call_compat) is not bool:
            raise ValueError(f"backend {name!r} ollama_tool_call_compat must be boolean")
        semantic_planner = table.get('semantic_system_planner', False)
        if type(semantic_planner) is not bool or (semantic_planner and capability != 'chat'):
            raise ValueError(f'backend {name!r} semantic_system_planner must be a chat boolean')
        grounded_planner = table.get('grounded_system_planner', False)
        if type(grounded_planner) is not bool or (grounded_planner and not semantic_planner):
            raise ValueError('grounded_system_planner requires semantic_system_planner=true')
        document_tasks = table.get('document_tasks', False)
        if type(document_tasks) is not bool or (document_tasks and not grounded_planner):
            raise ValueError('document_tasks requires grounded_system_planner=true')

        on_demand = table.get('on_demand', False)
        if type(on_demand) is not bool:
            raise ValueError('on_demand must be boolean')

        idle_timeout = table.get('idle_timeout_seconds', 0)
        if type(idle_timeout) not in (int, float) or not math.isfinite(idle_timeout) or not 0 <= idle_timeout <= 120:
            raise ValueError('idle_timeout_seconds must be a finite number between 0 and 120')

        backend = BackendConfig(
            name=name,
            capability=capability,
            url=url,
            enabled=bool(table.get("enabled", True)),
            required=bool(table.get("required", False)),
            health_url=expand_env(str(table.get("health_url", "")).strip()),
            models=_as_string_tuple(table.get("models"), f"backend {name}.models"),
            upstream_model=str(table.get("upstream_model", "")).strip(),
            allowed_tools=_as_string_tuple(
                table.get("allowed_tools"), f"backend {name}.allowed_tools"
            ),
            max_system_prompt_chars=int(table.get("max_system_prompt_chars", 0)),
            system_prompt_override=str(table.get("system_prompt_override", "")),
            ollama_tool_call_compat=tool_call_compat,
            semantic_system_planner=semantic_planner,
            grounded_system_planner=grounded_planner,
            document_tasks=document_tasks,
            system_planner_backend=str(table.get('system_planner_backend', '')).strip(),
            reading_summary_backend=str(table.get('reading_summary_backend', '')).strip(),
            on_demand=on_demand,
            idle_timeout_seconds=float(idle_timeout),
            api_key_env=str(table.get("api_key_env", "")).strip(),
            timeout_seconds=float(table.get("timeout_seconds", 300)),
            health_timeout_seconds=float(table.get("health_timeout_seconds", 2)),
            max_concurrency=int(table.get("max_concurrency", 1)),
            acquire_timeout_seconds=float(table.get("acquire_timeout_seconds", 1)),
            priority=int(table.get("priority", 50)),
            resource_group=resource_group,
            managed=managed,
            command=tuple(expand_env(part) for part in command),
            working_directory=expand_env(
                str(table.get("working_directory", "")).strip()
            ),
            environment=environment,
            restart_on_failure=bool(table.get("restart_on_failure", True)),
            restart_backoff_seconds=float(table.get("restart_backoff_seconds", 2)),
            max_restarts=int(table.get("max_restarts", 5)),
            exclusive_with=_as_string_tuple(table.get('exclusive_with'), f'backend {name}.exclusive_with'),
        )
        if backend.timeout_seconds <= 0 or backend.health_timeout_seconds <= 0:
            raise ValueError(f"backend {name!r} timeouts must be positive")
        if backend.max_concurrency <= 0:
            raise ValueError(f"backend {name!r} max_concurrency must be positive")
        if backend.acquire_timeout_seconds < 0:
            raise ValueError(
                f"backend {name!r} acquire_timeout_seconds must not be negative"
            )
        if backend.max_system_prompt_chars < 0:
            raise ValueError(
                f"backend {name!r} max_system_prompt_chars must not be negative"
            )
        if not 0 <= backend.priority <= 100:
            raise ValueError(f"backend {name!r} priority must be between 0 and 100")
        if backend.restart_backoff_seconds < 0 or backend.max_restarts < 0:
            raise ValueError(f"backend {name!r} restart settings must not be negative")
        if backend.on_demand and (not backend.managed or backend.required or not backend.health_url or backend.max_concurrency != 1):
            raise ValueError('on_demand needs a managed optional backend, health_url and max_concurrency=1')
        if backend.idle_timeout_seconds and not backend.on_demand:
            raise ValueError('idle_timeout_seconds requires on_demand=true')
        backends.append(backend)

    if not backends:
        raise ValueError("at least one backend must be configured")
    for backend in backends:
        if backend.exclusive_with:
            if (not backend.managed or not backend.enabled or backend.capability != 'chat'
                    or not backend.health_url or backend.max_concurrency != 1
                    or resource_groups.get(backend.resource_group) != 1
                    or len(set(backend.exclusive_with)) != len(backend.exclusive_with)):
                raise ValueError('exclusive_with requires enabled managed serial chat backends in a serial resource group')
            for name in backend.exclusive_with:
                peer = next((item for item in backends if item.name == name), None)
                if (peer is None or peer.name == backend.name or not peer.enabled or not peer.managed
                        or peer.capability != 'chat' or peer.resource_group != backend.resource_group
                        or backend.name not in peer.exclusive_with):
                    raise ValueError('exclusive_with must name symmetric managed chat peers in the same resource group')
        if backend.reading_summary_backend:
            reader = next((item for item in backends if item.name == backend.reading_summary_backend), None)
            if backend.capability != 'chat' or reader is None or not reader.enabled or reader.capability != 'chat' or reader.name == backend.name:
                raise ValueError('reading_summary_backend must reference a different enabled chat backend')
        if backend.system_planner_backend:
            planner = next((item for item in backends if item.name == backend.system_planner_backend), None)
            if not backend.semantic_system_planner or planner is None or not planner.enabled or planner.capability != 'chat':
                raise ValueError('system_planner_backend must reference an enabled chat backend in planner mode')
    return AdapterConfig(
        server=server,
        backends=tuple(backends),
        resource_groups=resource_groups,
    )
