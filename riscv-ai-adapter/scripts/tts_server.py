"""Loopback-only resident TTS worker for the Adapter speech endpoint.

Uses the existing RISC-V runtime, without installing another ORT or using cloud
speech. Native bridges support VITS/Melo and Matcha; VITS also has a CLI fallback.
"""
from __future__ import annotations

import argparse
import io
import json
import logging
import math
import os
import re
import select
import socket
import subprocess
import tempfile
import threading
import time
import wave
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

LOG = logging.getLogger("kylinbot-tts")
MAX_REQUEST = 16384
MAX_TEXT = 2000
MAX_AUDIO = 32 * 1024 * 1024
PROFILES = {
    "aishell3": ("vits-icefall-zh-aishell3", 174, 33),
    "melo-zh-en": ("vits-melo-tts-zh_en", 1, 0),
    "matcha-zh-en": ("matcha-icefall-zh-en", 1, 0),
}


def parse_request(body: bytes, profile: str = "aishell3") -> tuple[str, int, float]:
    model_name, speakers, default_sid = PROFILES[profile]
    value = json.loads(body)
    if not isinstance(value, dict):
        raise ValueError("expected a JSON object")
    text = value.get("input")
    if not isinstance(text, str) or not text.strip() or len(text) > MAX_TEXT:
        raise ValueError("input must contain 1 to 2000 characters")
    if any(ord(ch) < 32 and ch not in "\n\r\t" for ch in text):
        raise ValueError("input contains unsupported control characters")
    # vits-zh remains the Adapter's stable model alias across TTS profiles.
    if value.get("model", "vits-zh") not in ("vits-zh", model_name, profile):
        raise ValueError("unsupported speech model")
    if value.get("response_format", "wav") != "wav":
        raise ValueError("only WAV output is supported")
    voice = value.get("voice", f"zh-{default_sid}")
    if voice == "alloy":
        voice = f"zh-{default_sid}"
    voice_error = f"voice must be zh-0 to zh-{speakers - 1}"
    if not isinstance(voice, str) or not voice.startswith("zh-"):
        raise ValueError(voice_error)
    try:
        sid = int(voice[3:])
    except ValueError:
        raise ValueError(voice_error) from None
    if not 0 <= sid < speakers:
        raise ValueError(voice_error)
    speed = value.get("speed", 1.0)
    if isinstance(speed, bool) or not isinstance(speed, (int, float)):
        raise ValueError("speed must be a finite number from 0.5 to 2.0")
    if not math.isfinite(speed) or not 0.5 <= speed <= 2.0:
        raise ValueError("speed must be a finite number from 0.5 to 2.0")
    return text.strip(), sid, float(speed)


def validate_melo_lexicon(extra: str, tokens: str) -> str:
    """Validate project-name phones/tones before passing them to the C++ CLI."""
    symbols = {line.rsplit(maxsplit=1)[0] for line in tokens.splitlines() if line.strip()}
    entries = []
    words = set()
    for line in extra.splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        fields = line.split()
        word, values = fields[0].lower(), fields[1:]
        if not re.fullmatch(r"[a-z][a-z0-9'-]{0,99}", word) or word in words:
            raise ValueError("invalid or duplicate project lexicon word")
        if not values or len(values) % 2:
            raise ValueError("project lexicon requires equal phone and tone counts")
        phones, tones = values[:len(values) // 2], values[len(values) // 2:]
        if any(phone not in symbols for phone in phones):
            raise ValueError("unknown project lexicon phoneme")
        if any(not tone.isascii() or not tone.isdigit() or not 0 <= int(tone) <= 50 for tone in tones):
            raise ValueError("invalid project lexicon tone")
        words.add(word)
        entries.append(" ".join([word, *phones, *tones]))
    if not entries:
        raise ValueError("empty project lexicon")
    return "\n".join(entries) + "\n"


def validate_wav(data: bytes) -> tuple[int, float]:
    if not data or len(data) > MAX_AUDIO:
        raise ValueError("invalid audio size")
    with wave.open(io.BytesIO(data), "rb") as audio:
        if audio.getcomptype() != "NONE" or audio.getsampwidth() != 2:
            raise ValueError("expected PCM16 WAV")
        if audio.getnchannels() not in (1, 2) or not 8000 <= audio.getframerate() <= 48000:
            raise ValueError("unsupported WAV audio format")
        frames = audio.getnframes()
        pcm = audio.readframes(frames)
        if frames <= 0 or len(pcm) != frames * audio.getnchannels() * 2:
            raise ValueError("empty or truncated WAV")
        return audio.getframerate(), frames / audio.getframerate()


class TtsServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, binary: Path | None, model_dir: Path, port: int, threads: int,
                 profile: str = "aishell3", extra_lexicon: Path | None = None,
                 native_bridge: Path | None = None, model_file: str = "model.onnx"):
        self.binary, self.model_dir, self.threads = binary, model_dir, threads
        self.profile = profile
        self.model_name, self.speakers, self.default_sid = PROFILES[profile]
        self.lexicon = model_dir / "lexicon.txt"
        self._lexicon_cache = None
        self.native_engine = None
        self.failed = False
        allowed = ("matcha-icefall-zh-en/model-steps-3.q.onnx",) if profile == "matcha-zh-en" else ("model.onnx", "model.int8.onnx")
        if model_file not in allowed:
            raise ValueError("unsupported model filename")
        self.model_file = model_file
        self.gate = threading.BoundedSemaphore(1)
        if profile == "matcha-zh-en":
            if native_bridge is None:
                raise ValueError("Matcha requires the resident native bridge")
            cache = os.environ.get("RVAI_TTS_CACHE_ROOT", "")
            if not cache or model_dir.resolve() != (Path(cache) / "models/tts/matcha-tts").resolve():
                raise ValueError("Matcha model/cache root mismatch")
            for name in ("date.fst", "phone.fst", "number.fst"):
                if not (Path(cache) / "models/tts/text_norm/v1/zh" / name).is_file():
                    raise FileNotFoundError("Matcha normalization rules are missing")
            if not (Path(cache) / "thirdparty/cpp-pinyin/res/dict/mandarin").exists():
                raise FileNotFoundError("Matcha pinyin dictionary is missing")
            assets = (model_file, "matcha-icefall-zh-en/vocab_tts.txt", "vocos-16khz-univ.q.onnx")
        else:
            if binary is None or not binary.is_file():
                raise FileNotFoundError(binary)
            assets = (model_file, "tokens.txt", "lexicon.txt", "phone.fst", "date.fst", "number.fst")
        for name in assets:
            if not (model_dir / name).is_file():
                raise FileNotFoundError(model_dir / name)
        if model_file == "model.int8.onnx" and (model_dir / model_file).stat().st_size < 1024:
            raise ValueError("INT8 model is too small; resolve the Git LFS pointer first")
        if extra_lexicon is not None:
            if profile != "melo-zh-en":
                raise ValueError("extra lexicon is only supported for MeloTTS")
            extra = validate_melo_lexicon(extra_lexicon.read_text(encoding="utf-8"),
                                         (model_dir / "tokens.txt").read_text(encoding="utf-8"))
            # Keep the downloaded model/lexicon untouched, with an ephemeral
            # combined lexicon prepared once per worker, not once per request.
            self._lexicon_cache = tempfile.TemporaryDirectory(prefix="kylinbot-tts-lexicon-")
            self.lexicon = Path(self._lexicon_cache.name) / "lexicon.txt"
            self.lexicon.write_text(extra + (model_dir / "lexicon.txt").read_text(encoding="utf-8"), encoding="utf-8")
        self.ready = False
        super().__init__(("127.0.0.1", port), TtsHandler)
        if native_bridge is not None:
            from tts_native import NativeEngine
            try:
                if not native_bridge.is_file():
                    raise FileNotFoundError(native_bridge)
                options = {"backend": "matcha"} if profile == "matcha-zh-en" else {}
                self.native_engine = NativeEngine(native_bridge, model_dir, self.lexicon, threads,
                                                  model_file=model_file, **options)
                if self.native_engine.speakers != self.speakers or not 8000 <= self.native_engine.sample_rate <= 48000:
                    raise ValueError("native TTS model/profile mismatch")
                if profile == "matcha-zh-en" and self.native_engine.sample_rate != 16000:
                    raise ValueError("Matcha requires 16kHz output")
            except BaseException:
                self.server_close()
                raise

    def server_close(self) -> None:
        super().server_close()
        if self.native_engine is not None:
            self.native_engine.close()
        if self._lexicon_cache is not None:
            self._lexicon_cache.cleanup()

    def synthesize(self, text: str, sid: int, speed: float, cancelled=None) -> bytes:
        if self.native_engine is not None:
            # Matcha's bilingual frontend has no token/mapping for Chinese 、.
            # Use its supported comma pause inside this same utterance. Keep
            # the original response and streaming clause boundaries untouched.
            if self.profile == "matcha-zh-en":
                text = text.replace("、", ",")
            options = {"cancelled": cancelled} if self.profile == "matcha-zh-en" else {}
            data = self.native_engine.synthesize(text, sid, speed, **options)
            validate_wav(data)
            return data
        with tempfile.TemporaryDirectory(prefix="kylinbot-tts-") as temporary:
            output = Path(temporary) / "speech.wav"
            command = [
                str(self.binary), "--provider=cpu", f"--num-threads={self.threads}",
                f"--vits-model={self.model_dir / self.model_file}",
                f"--vits-tokens={self.model_dir / 'tokens.txt'}",
                f"--vits-lexicon={self.lexicon}",
                "--tts-rule-fsts=" + ",".join(str(self.model_dir / name) for name in ("phone.fst", "date.fst", "number.fst")),
                f"--sid={sid}", f"--speed={speed}", f"--output-filename={output}",
                # Prefix a space so user text cannot be parsed as a CLI option.
                " " + text,
            ]
            completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120, check=False)
            if completed.returncode != 0:
                LOG.error("sherpa process failed: status=%d", completed.returncode)
                raise RuntimeError("speech synthesis process failed")
            oov_count = completed.stderr.count(b"OOV")
            if oov_count:
                # Report the count only; stderr can contain private input text.
                LOG.warning("speech frontend reported %d OOV warnings", oov_count)
            if not output.is_file() or output.stat().st_size > MAX_AUDIO:
                raise ValueError("speech synthesis produced invalid audio")
            data = output.read_bytes()
            validate_wav(data)
            return data


class TtsHandler(BaseHTTPRequestHandler):
    server: TtsServer
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt: str, *args: object) -> None:
        LOG.info("%s - %s", self.client_address[0], fmt % args)

    def send_body(self, status: int, data: bytes, content_type: str) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def error(self, status: int, message: str) -> None:
        self.send_body(status, json.dumps({"error": {"message": message}}).encode(), "application/json")

    def do_GET(self) -> None:  # noqa: N802
        if self.path != "/health":
            self.error(404, "unknown endpoint")
            return
        try:
            self.send_body(200 if self.server.ready else 503, json.dumps({
                "status": "ready" if self.server.ready else "warming",
                "backend": "spacemit-matcha-cpu" if self.server.profile == "matcha-zh-en" else "sherpa-onnx-vits", "model": self.server.model_name,
                "languages": ["zh", "en"] if self.server.profile in ("melo-zh-en", "matcha-zh-en") else ["zh"],
                "model_resident": self.server.native_engine is not None,
                "model_file": self.server.model_file,
                "provider": "cpu", "response_format": "wav",
                "threads": self.server.threads,
                "text_normalization": "fst" if self.server.profile == "matcha-zh-en" else "model-rules",
            }).encode(), "application/json")
        except (BrokenPipeError, ConnectionResetError):
            self.close_connection = True

    def do_POST(self) -> None:  # noqa: N802
        if self.path != "/v1/audio/speech":
            self.close_connection = True
            self.error(404, "unknown endpoint")
            return
        try:
            if self.headers.get("Transfer-Encoding"):
                raise ValueError("chunked requests are not supported")
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= MAX_REQUEST:
                self.close_connection = True
                self.error(413, "invalid request size")
                return
            self.connection.settimeout(10)
            body = self.rfile.read(length)
            if len(body) != length:
                raise ValueError("truncated request")
            text, sid, speed = parse_request(body, self.server.profile)
        except (ValueError, UnicodeDecodeError, TimeoutError):
            self.close_connection = True
            self.error(400, "invalid speech request")
            return
        if not self.server.ready:
            self.error(503, "speech worker is not ready")
            return
        if not self.server.gate.acquire(blocking=False):
            self.error(429, "speech worker is busy")
            return
        started = time.monotonic()
        try:
            options = {"cancelled": self.client_disconnected} if self.server.profile == "matcha-zh-en" else {}
            data = self.server.synthesize(text, sid, speed, **options)
            rate, duration = validate_wav(data)
            # Do not log user text or persist generated audio.
            LOG.info("synthesis complete: chars=%d rate=%d audio_seconds=%.2f elapsed_seconds=%.2f", len(text), rate, duration, time.monotonic() - started)
            self.send_body(200, data, "audio/wav")
        except (BrokenPipeError, ConnectionResetError):
            LOG.info("speech client disconnected")
        except InterruptedError:
            LOG.info("speech synthesis cancelled at clause boundary")
            self.close_connection = True
        except subprocess.TimeoutExpired:
            self.error(504, "speech synthesis timed out")
        except (OSError, RuntimeError, ValueError, wave.Error):
            LOG.exception("speech synthesis failed")
            self.error(502, "speech synthesis failed")
        finally:
            self.server.gate.release()
            if self.server.native_engine is not None and not self.server.native_engine.is_alive():
                # An inference timeout/crash must not leave a fake-ready service.
                # Stop the HTTP loop so systemd can restart and reload the model.
                self.server.ready = False
                self.server.failed = True
                self.server.shutdown()

    def client_disconnected(self) -> bool:
        try:
            readable, _, _ = select.select([self.connection], [], [], 0)
            return bool(readable) and self.connection.recv(1, socket.MSG_PEEK) == b""
        except (ConnectionError, OSError):
            return True


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--model-dir", required=True, type=Path)
    parser.add_argument("--profile", choices=PROFILES, default="aishell3")
    parser.add_argument("--extra-lexicon", type=Path)
    parser.add_argument("--native-bridge", type=Path)
    parser.add_argument("--model-file", default="model.onnx")
    parser.add_argument("--port", type=int, default=18083)
    parser.add_argument("--threads", type=int, default=2)
    args = parser.parse_args()
    if not 1 <= args.threads <= 8 or not 1 <= args.port <= 65535:
        parser.error("threads must be 1..8 and port must be 1..65535")
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
    server = TtsServer(args.binary, args.model_dir, args.port, args.threads,
                       args.profile, args.extra_lexicon, args.native_bridge, args.model_file)
    try:
        warmup = "Hello，语音服务已就绪。" if args.profile in ("melo-zh-en", "matcha-zh-en") else "语音服务已就绪。"
        server.synthesize(warmup, server.default_sid, 1.0)
        server.ready = True
        LOG.info("TTS worker ready on loopback port %d: model=%s", args.port, server.model_name)
        server.serve_forever()
        if server.failed:
            raise RuntimeError("native TTS worker stopped; restart required")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
