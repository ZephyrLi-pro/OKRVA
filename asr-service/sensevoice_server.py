#!/usr/bin/env python3
"""OpenAI-compatible local ASR bridge for SpacemiT SenseVoice."""

from __future__ import annotations

import json
import os
import pathlib
import tempfile
import threading
from email.parser import BytesParser
from email.policy import default as email_policy
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import spacemit_asr
import numpy as np


LANGUAGES = {
    "auto": spacemit_asr.Language.AUTO,
    "zh": spacemit_asr.Language.ZH,
    "en": spacemit_asr.Language.EN,
    "ja": spacemit_asr.Language.JA,
    "ko": spacemit_asr.Language.KO,
    "yue": spacemit_asr.Language.YUE,
}


class AsrEngine:
    def __init__(
        self,
        model_dir: str,
        provider: str,
        language: str,
        threads: int,
        warmup_seconds: float,
    ) -> None:
        config = spacemit_asr.Config.sensevoice(model_dir, enable_emotion=False)
        config.language = LANGUAGES.get(language.lower(), spacemit_asr.Language.AUTO)
        config.punctuation_enabled = True
        # The voice channel already segments the utterance. Do not crop it again
        # at the backend's fixed -45 dBFS threshold: that can discard quiet words
        # surrounding a louder syllable. Keep the complete supplied recording.
        config._config.vad_enabled = False
        config.provider = provider
        if hasattr(config._config, "num_threads"):
            config._config.num_threads = threads

        self.provider = provider
        self.lock = threading.Lock()
        self.engine = spacemit_asr.Engine(config).initialize()
        if warmup_seconds > 0:
            samples = np.zeros(int(config.sample_rate * warmup_seconds), dtype=np.float32)
            self.engine.recognize(samples)

    def transcribe(self, audio_path: str) -> str:
        with self.lock:
            return self.engine.recognize_file(audio_path).text.strip()

    def close(self) -> None:
        self.engine.shutdown()


ENGINE: AsrEngine
BEARER_TOKEN = ""
MAX_AUDIO_BYTES = 25 * 1024 * 1024


class Handler(BaseHTTPRequestHandler):
    server_version = "RiscvAiAdapterSenseVoice/1.0"

    def send_json(self, status: int, payload: dict[str, object]) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if self.path == "/health":
            self.send_json(
                200,
                {
                    "status": "ready",
                    "backend": "sensevoice",
                    "provider": ENGINE.provider,
                    "endpoint_trimming": False,
                },
            )
        else:
            self.send_json(404, {"error": "not found"})

    def do_POST(self) -> None:
        if self.path not in ("/", "/v1/audio/transcriptions"):
            self.send_json(404, {"error": "not found"})
            return
        if self.headers.get("Authorization") != f"Bearer {BEARER_TOKEN}":
            self.send_json(401, {"error": "unauthorized"})
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            length = 0
        if length <= 0 or length > MAX_AUDIO_BYTES + 1024 * 1024:
            self.send_json(413, {"error": "audio request is too large"})
            return

        try:
            content_type = self.headers.get("Content-Type", "")
            if not content_type.lower().startswith("multipart/form-data"):
                raise ValueError("request must use multipart/form-data")
            body = self.rfile.read(length)
            if len(body) != length:
                raise ValueError("incomplete request body")
            message = BytesParser(policy=email_policy).parsebytes(
                b"Content-Type: "
                + content_type.encode("ascii", "strict")
                + b"\r\nMIME-Version: 1.0\r\n\r\n"
                + body
            )
            file_part = next(
                (
                    part
                    for part in message.iter_parts()
                    if part.get_param("name", header="content-disposition") == "file"
                ),
                None,
            )
            if file_part is None:
                raise ValueError("multipart field 'file' is required")
            audio = file_part.get_payload(decode=True)
            if audio is None:
                raise ValueError("multipart field 'file' is empty")
            if len(audio) > MAX_AUDIO_BYTES:
                raise ValueError("audio file is too large")

            filename = file_part.get_filename() or "audio.wav"
            suffix = pathlib.Path(filename).suffix.lower()
            if suffix not in {".wav", ".mp3", ".flac", ".ogg", ".m4a"}:
                suffix = ".wav"
            with tempfile.NamedTemporaryFile(suffix=suffix) as audio_file:
                audio_file.write(audio)
                audio_file.flush()
                text = ENGINE.transcribe(audio_file.name)
            self.send_json(200, {"text": text})
        except Exception as error:
            self.send_json(400, {"error": str(error)})

    def log_message(self, fmt: str, *args: object) -> None:
        print(f"{self.client_address[0]} {fmt % args}", flush=True)


def main() -> None:
    global ENGINE, BEARER_TOKEN, MAX_AUDIO_BYTES
    model_dir_value = os.environ.get("SENSEVOICE_MODEL_DIR") or os.environ["KYLINBOT_ASR_MODEL_DIR"]
    model_dir = pathlib.Path(os.path.expanduser(model_dir_value))
    provider = os.environ.get("SENSEVOICE_PROVIDER", "spacemit")
    language = os.environ.get("SENSEVOICE_LANGUAGE", "auto")
    threads = int(os.environ.get("SENSEVOICE_THREADS", "2"))
    warmup_seconds = float(os.environ.get("SENSEVOICE_WARMUP_SECONDS", "0.5"))
    port = int(os.environ.get("SENSEVOICE_PORT", "18083"))
    BEARER_TOKEN = os.environ["KYLINBOT_ASR_TOKEN"]
    MAX_AUDIO_BYTES = int(os.environ.get("SENSEVOICE_MAX_BYTES", str(MAX_AUDIO_BYTES)))

    ENGINE = AsrEngine(str(model_dir), provider, language, threads, warmup_seconds)
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print(
        f"SenseVoice ASR ready on http://127.0.0.1:{port} "
        f"(provider={provider}, language={language})",
        flush=True,
    )
    try:
        server.serve_forever()
    finally:
        server.server_close()
        ENGINE.close()


if __name__ == "__main__":
    main()
