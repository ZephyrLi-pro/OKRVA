from __future__ import annotations

import argparse
import hmac
import json
import logging
import os
import platform
import re
import select
import socket
import time
import urllib.error
import uuid
from http.client import HTTPException
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from dataclasses import replace

from . import __version__
from .backends import (
    BackendBusyError,
    BackendRegistry,
    BackendUnavailableError,
    ProcessManager,
    explicit_system_intent,
    requested_model_from_json,
    rewrite_chat_request,
    standalone_conversation_request,
)
from .config import AdapterConfig, load_config
from .metrics import Metrics
from .monitor import SystemMonitor
from .semantic_planner import SemanticPlanner
from .grounding import GroundedPlanner
from .reference_context import reference_chat_request, analysis_chat_request, page_summary_backend


LOG = logging.getLogger("riscv-ai-adapterd")
MODEL_READY_RE = re.compile(r"^/v2/models/([^/]+)/ready$")
BACKEND_ACTION_RE = re.compile(r"^/admin/backends/([^/]+)/(start|stop|restart)$")

ROUTES = {
    "/v1/chat/completions": "chat",
    "/v1/audio/transcriptions": "transcription",
    "/v1/audio/speech": "speech",
    "/v1/embeddings": "embedding",
    "/v1/ocr": "ocr",
}


class AdapterHTTPServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(
        self,
        config: AdapterConfig,
        process_manager: ProcessManager | None = None,
    ):
        self.config = config
        self.metrics = Metrics()
        self.process_manager = process_manager or ProcessManager(config.backends)
        self.registry = BackendRegistry(config, self.metrics, self.process_manager)
        self.monitor = SystemMonitor()
        self.semantic_planner = SemanticPlanner()
        self.grounded_planner = GroundedPlanner()
        super().__init__((config.server.host, config.server.port), AdapterHandler)


class AdapterHandler(BaseHTTPRequestHandler):
    server: AdapterHTTPServer
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt: str, *args: object) -> None:
        LOG.info("%s - %s", self.client_address[0], fmt % args)

    def _send_json(self, status: int, value: Any) -> None:
        body = json.dumps(value, ensure_ascii=False).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _send_system_intent(self, intent: dict[str, object], body: bytes, backend_name: str) -> None:
        """Native intent dispatch, not an inferred success or a desktop write."""
        request = json.loads(body)
        message: dict[str, object] = {"role": "assistant", "content": intent.get("content")}
        finish = "stop"
        if "name" in intent:
            message["tool_calls"] = [{"id": intent.get('id') or "call_" + uuid.uuid4().hex, "type": "function", "function": {
                "name": intent["name"], "arguments": json.dumps(intent["arguments"], ensure_ascii=False)}}]
            finish = "tool_calls"
        route = intent.get('route', 'explicit-system-intent')
        value = {"id": "chatcmpl-" + uuid.uuid4().hex, "created": int(time.time()),
                 "model": request.get("model", ""), "rvai_route": route}
        if 'planner_model' in intent:
            value['rvai_planner_model'] = intent['planner_model']
            value['rvai_planner_backend'] = intent['planner_backend']
        if request.get("stream") is True:
            delta = dict(message)
            if "tool_calls" in delta:
                delta["tool_calls"] = [dict(delta["tool_calls"][0], index=0)]
            events = [dict(value, object="chat.completion.chunk", choices=[{
                "index": 0, "delta": delta, "finish_reason": None}]),
                dict(value, object="chat.completion.chunk", choices=[{
                    "index": 0, "delta": {}, "finish_reason": finish}])]
            wire = b"".join(b"data: " + json.dumps(event, ensure_ascii=False).encode() + b"\n\n" for event in events)
            wire += b"data: [DONE]\n\n"
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Content-Length", str(len(wire)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-RVAI-Backend", backend_name)
            self.send_header("X-RVAI-Route", str(route))
            self.end_headers()
            self.wfile.write(wire)
            self.wfile.flush()
        else:
            value.update(object="chat.completion", choices=[{"index": 0, "message": message, "finish_reason": finish}])
            self._send_json(200, value)
        LOG.info("chat route: %s; execution remains in KylinBot/MCP", route)

    def _authorized(self) -> bool:
        token_env = self.server.config.server.token_env
        if not token_env:
            return True
        expected = os.environ.get(token_env, "")
        header = self.headers.get("Authorization", "")
        supplied = header[7:] if header.startswith("Bearer ") else ""
        return bool(expected) and hmac.compare_digest(expected, supplied)

    def _require_authorization(self) -> bool:
        if self._authorized():
            return True
        self._send_json(
            401,
            {
                "error": {
                    "type": "authentication_error",
                    "message": "missing or invalid bearer token",
                }
            },
        )
        return False

    def client_disconnected(self) -> bool:
        try:
            readable, _, _ = select.select([self.connection], [], [], 0)
            return bool(readable) and self.connection.recv(1, socket.MSG_PEEK) == b""
        except (OSError, ValueError):
            return True

    def do_GET(self) -> None:  # noqa: N802
        path = self.path.split("?", 1)[0]
        if path in {"/health", "/v1/health"}:
            ready, backends = self.server.registry.readiness()
            self._send_json(
                200 if ready else 503,
                {
                    "status": "ready" if ready else "degraded",
                    "version": __version__,
                    "architecture": platform.machine(),
                    "backends": backends,
                    "scheduler": self.server.registry.scheduler.snapshot(),
                    "processes": self.server.process_manager.snapshot(),
                },
            )
            return
        if path == "/v2/health/live":
            self._send_json(200, {"live": True})
            return
        if path == "/v2/health/ready":
            ready, _ = self.server.registry.readiness()
            self._send_json(200 if ready else 503, {"ready": ready})
            return
        if path == "/v2":
            self._send_json(
                200,
                {
                    "name": "riscv-ai-adapter",
                    "version": __version__,
                    "extensions": ["openai-compatible-routing", "backend-readiness"],
                },
            )
            return
        model_match = MODEL_READY_RE.match(path)
        if model_match:
            model = model_match.group(1)
            backend = next(
                (
                    item
                    for item in self.server.registry.enabled_backends
                    if model in item.models
                ),
                None,
            )
            if backend is None:
                self._send_json(404, {"name": model, "ready": False})
                return
            ready, detail = self.server.registry.probe(backend)
            self._send_json(
                200 if ready else 503,
                {"name": model, "ready": ready, "detail": detail},
            )
            return
        if path == "/v1/models":
            if not self._require_authorization():
                return
            self._send_json(
                200,
                {"object": "list", "data": self.server.registry.model_entries()},
            )
            return
        if path == "/v1/system/info":
            if not self._require_authorization():
                return
            self._send_json(
                200,
                self.server.monitor.snapshot(
                    self.server.process_manager.process_ids()
                ),
            )
            return
        if path == "/v1/scheduler":
            if not self._require_authorization():
                return
            self._send_json(200, self.server.registry.scheduler.snapshot())
            return
        if path == "/admin/backends":
            if not self._require_authorization():
                return
            self._send_json(200, {"data": self.server.process_manager.snapshot()})
            return
        if path == "/metrics":
            if not self._require_authorization():
                return
            system_snapshot = self.server.monitor.snapshot(
                self.server.process_manager.process_ids()
            )
            body = (
                self.server.metrics.render_prometheus()
                + self.server.monitor.render_prometheus(system_snapshot)
            ).encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; version=0.0.4")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        self._send_json(404, {"error": {"message": "route not found"}})

    def do_POST(self) -> None:  # noqa: N802
        if not self._require_authorization():
            return
        path = self.path.split("?", 1)[0]
        action_match = BACKEND_ACTION_RE.match(path)
        if action_match:
            name, action = action_match.groups()
            try:
                if action == "start":
                    self.server.process_manager.start_one(name)
                elif action == "stop":
                    self.server.process_manager.stop_one(name)
                else:
                    self.server.process_manager.restart_one(name)
            except KeyError as error:
                self._send_json(404, {"error": {"message": str(error)}})
                return
            except (OSError, ValueError) as error:
                self._send_json(409, {"error": {"message": str(error)}})
                return
            item = next(
                entry
                for entry in self.server.process_manager.snapshot()
                if entry["name"] == name
            )
            self._send_json(200, item)
            return
        capability = ROUTES.get(path)
        if capability is None:
            self._send_json(404, {"error": {"message": "route not found"}})
            return

        content_length_text = self.headers.get("Content-Length")
        if content_length_text is None:
            self._send_json(411, {"error": {"message": "Content-Length is required"}})
            return
        try:
            content_length = int(content_length_text)
        except ValueError:
            self._send_json(400, {"error": {"message": "invalid Content-Length"}})
            return
        if content_length < 0 or content_length > self.server.config.server.request_limit_bytes:
            self._send_json(413, {"error": {"message": "request body is too large"}})
            return
        body = self.rfile.read(content_length)
        requested_model = requested_model_from_json(body) if capability in {"chat", "embedding"} else ""

        response_started = False
        try:
            backend = self.server.registry.select(capability, requested_model)
            priority_text = self.headers.get("X-RVAI-Priority", "").strip()
            try:
                priority = int(priority_text) if priority_text else backend.priority
            except ValueError:
                self._send_json(400, {"error": {"message": "invalid X-RVAI-Priority"}})
                return
            if not 0 <= priority <= 100:
                self._send_json(
                    400,
                    {"error": {"message": "X-RVAI-Priority must be between 0 and 100"}},
                )
                return
            if capability == "chat":
                reference_body = None
                conversation_body = standalone_conversation_request(body,
                    backend.upstream_model, backend.allowed_tools,
                    backend.max_system_prompt_chars, backend.system_prompt_override)
                if conversation_body is not None:
                    body = conversation_body
                    intent = None
                    LOG.info('chat route: standalone conversation before model planning')
                elif backend.semantic_system_planner:
                    planner_config = next((item for item in self.server.config.backends
                        if item.name == backend.system_planner_backend), backend)
                    def infer_plan(payload):
                        planner_backend = replace(planner_config, semantic_system_planner=True,
                            timeout_seconds=min(planner_config.timeout_seconds, 120))
                        with self.server.registry.open(planner_backend, json.dumps(payload, ensure_ascii=False).encode(),
                                'application/json', 'application/json', priority, cancelled=self.client_disconnected) as upstream:
                            if upstream.status != 200:
                                raise BackendUnavailableError('Model planner upstream failed')
                            raw = upstream.stream.read(128 * 1024 + 1)
                            if len(raw) > 128 * 1024:
                                raise BackendUnavailableError('Model planner response exceeds limit')
                            result = json.loads(raw)
                            try:
                                selected = json.loads(result['choices'][0]['message']['content']).get('intent')
                                if selected in ('analyze','agent','chat','operate','documents','report_cleanup','presentation'):
                                    LOG.info('private model plan: intent=%s', selected)
                            except (ValueError,TypeError,KeyError,IndexError,AttributeError):
                                pass
                            return result
                    def infer_presentation(payload):
                        # A direct registry lease, after planner inference has closed.
                        # Never re-enter this HTTP adapter while holding a model slot.
                        name = backend.reading_summary_backend
                        notes_backend = next((item for item in self.server.config.backends
                            if item.name == name and item.enabled and item.capability == 'chat'), planner_config)
                        request = dict(payload, model=notes_backend.upstream_model or payload['model'])
                        with self.server.registry.open(notes_backend, json.dumps(request, ensure_ascii=False).encode(),
                                'application/json', 'application/json', priority, cancelled=self.client_disconnected) as upstream:
                            if upstream.status != 200:
                                raise BackendUnavailableError('Speaker-notes model upstream failed')
                            raw = upstream.stream.read(128 * 1024 + 1)
                            if len(raw) > 128 * 1024:
                                raise BackendUnavailableError('Speaker-notes response exceeds limit')
                            return json.loads(raw)
                    planner_model = planner_config.upstream_model or (planner_config.models[0] if planner_config.models else requested_model)
                    planner_engine = self.server.grounded_planner if backend.grounded_system_planner else self.server.semantic_planner
                    intent = planner_engine.handle(body, planner_model,
                        backend.allowed_tools, infer_plan, self.client_disconnected,
                        **({'document_tasks':backend.document_tasks,'infer_presentation':infer_presentation} if backend.grounded_system_planner else {}))
                    if intent is not None:
                        intent.update(planner_model=planner_model, planner_backend=planner_config.name)
                else:
                    intent = explicit_system_intent(body, backend.allowed_tools, bool(backend.system_prompt_override))
                if intent is not None and intent.get('read_only_analysis'):
                    summary_backend = page_summary_backend(body, backend, self.server.config.backends)
                    if summary_backend is not None:
                        backend = summary_backend
                        LOG.info('chat route: configured page summary model %s', backend.name)
                    reference_body = reference_chat_request(body, backend.upstream_model, narrative=summary_backend is not None)
                    if reference_body is None:
                        reference_body = analysis_chat_request(body, backend.upstream_model)
                    intent = None
                    LOG.info('chat route: model-selected analysis (no tools)')
                if intent is not None:
                    self._send_system_intent(intent, body, backend.name)
                    return
                body = reference_body if reference_body is not None else rewrite_chat_request(
                    body,
                    backend.upstream_model,
                    backend.allowed_tools,
                    backend.max_system_prompt_chars,
                    backend.system_prompt_override,
                    backend.ollama_tool_call_compat,
                    semantic_only=backend.semantic_system_planner,
                )
            elif capability == "embedding":
                body = rewrite_chat_request(body, backend.upstream_model)
            with self.server.registry.open(
                backend,
                body,
                self.headers.get("Content-Type", ""),
                self.headers.get("Accept", ""),
                priority,
                **({"cancelled": self.client_disconnected} if capability == "speech" or backend.on_demand else {}),
            ) as upstream:
                self.send_response(upstream.status)
                content_type = upstream.headers.get("Content-Type")
                if content_type:
                    self.send_header("Content-Type", content_type)
                self.send_header("X-RVAI-Backend", backend.name)
                upstream_length = upstream.headers.get("Content-Length")
                # HTTPResponse.read(n) can wait for n bytes or EOF. Small SSE
                # events must be forwarded as they arrive, not after generation.
                read_chunk = getattr(upstream.stream, "read1", upstream.stream.read)
                if upstream_length:
                    self.send_header("Content-Length", upstream_length)
                    self.end_headers()
                    response_started = True
                    while True:
                        chunk = read_chunk(64 * 1024)
                        if not chunk:
                            break
                        self.wfile.write(chunk)
                        self.wfile.flush()
                else:
                    self.send_header("Transfer-Encoding", "chunked")
                    self.end_headers()
                    response_started = True
                    while True:
                        chunk = read_chunk(64 * 1024)
                        if not chunk:
                            break
                        self.wfile.write(f"{len(chunk):X}\r\n".encode())
                        self.wfile.write(chunk)
                        self.wfile.write(b"\r\n")
                        self.wfile.flush()
                    self.wfile.write(b"0\r\n\r\n")
        except (BrokenPipeError, ConnectionResetError):
            # A cancelled client must not receive a second error response.
            # Leaving the registry context closes the upstream and its lease.
            self.close_connection = True
            LOG.info("client disconnected from backend response")
        except BackendBusyError as error:
            self._send_json(
                429,
                {"error": {"type": "backend_busy", "message": str(error)}},
            )
        except BackendUnavailableError as error:
            self._send_json(
                503,
                {"error": {"type": "backend_unavailable", "message": str(error)}},
            )
        except (OSError, TimeoutError, urllib.error.URLError, HTTPException) as error:
            LOG.warning("backend transport error: %s", error)
            if response_started:
                # Headers/status are already on the wire. Appending an HTTP
                # 502 to an SSE/chunked body corrupts the response. Close it
                # without a success terminator so clients detect truncation.
                self.close_connection = True
            else:
                try:
                    self._send_json(
                        502,
                        {"error": {"type": "backend_transport_error", "message": str(error)}},
                    )
                except (BrokenPipeError, ConnectionResetError):
                    self.close_connection = True


def make_server(
    config: AdapterConfig,
    process_manager: ProcessManager | None = None,
) -> AdapterHTTPServer:
    return AdapterHTTPServer(config, process_manager)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="RISC-V AI Adapter daemon")
    parser.add_argument(
        "--config",
        default="/etc/riscv-ai-adapter/config.toml",
        help="path to the TOML configuration",
    )
    parser.add_argument("--log-level", default="INFO")
    args = parser.parse_args(argv)
    logging.basicConfig(
        level=getattr(logging, args.log_level.upper(), logging.INFO),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )

    config = load_config(args.config)
    process_manager = ProcessManager(config.backends)
    server = make_server(config, process_manager)
    process_manager.start()
    LOG.info(
        "riscv-ai-adapterd %s listening on http://%s:%d",
        __version__,
        config.server.host,
        config.server.port,
    )
    try:
        server.serve_forever(poll_interval=0.5)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        process_manager.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
