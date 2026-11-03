#!/usr/bin/env python3
"""Small Whisper-compatible HTTP bridge for sherpa-onnx on the SpacemiT K1."""

from __future__ import annotations

import ctypes
import json
import os
import pathlib
import tempfile
import threading
from email.parser import BytesParser
from email.policy import default as email_policy
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class SherpaOnnxFeatureConfig(ctypes.Structure):
    _fields_ = [("sample_rate", ctypes.c_int32), ("feature_dim", ctypes.c_int32)]


class SherpaOnnxOfflineTransducerModelConfig(ctypes.Structure):
    _fields_ = [
        ("encoder", ctypes.c_char_p),
        ("decoder", ctypes.c_char_p),
        ("joiner", ctypes.c_char_p),
    ]


class SherpaOnnxOfflineParaformerModelConfig(ctypes.Structure):
    _fields_ = [("model", ctypes.c_char_p)]


class SherpaOnnxOfflineNemoEncDecCtcModelConfig(ctypes.Structure):
    _fields_ = [("model", ctypes.c_char_p)]


class SherpaOnnxOfflineWhisperModelConfig(ctypes.Structure):
    _fields_ = [
        ("encoder", ctypes.c_char_p),
        ("decoder", ctypes.c_char_p),
        ("language", ctypes.c_char_p),
        ("task", ctypes.c_char_p),
        ("tail_paddings", ctypes.c_int32),
        ("enable_token_timestamps", ctypes.c_int32),
        ("enable_segment_timestamps", ctypes.c_int32),
    ]


class SherpaOnnxOfflineCanaryModelConfig(ctypes.Structure):
    _fields_ = [
        ("encoder", ctypes.c_char_p),
        ("decoder", ctypes.c_char_p),
        ("src_lang", ctypes.c_char_p),
        ("tgt_lang", ctypes.c_char_p),
        ("use_pnc", ctypes.c_int32),
    ]


class SherpaOnnxOfflineCohereTranscribeModelConfig(ctypes.Structure):
    _fields_ = [
        ("encoder", ctypes.c_char_p),
        ("decoder", ctypes.c_char_p),
        ("language", ctypes.c_char_p),
        ("use_punct", ctypes.c_int32),
        ("use_itn", ctypes.c_int32),
    ]


class SherpaOnnxOfflineFireRedAsrModelConfig(ctypes.Structure):
    _fields_ = [("encoder", ctypes.c_char_p), ("decoder", ctypes.c_char_p)]


class SherpaOnnxOfflineFireRedAsrCtcModelConfig(ctypes.Structure):
    _fields_ = [("model", ctypes.c_char_p)]


class SherpaOnnxOfflineMoonshineModelConfig(ctypes.Structure):
    _fields_ = [
        ("preprocessor", ctypes.c_char_p),
        ("encoder", ctypes.c_char_p),
        ("uncached_decoder", ctypes.c_char_p),
        ("cached_decoder", ctypes.c_char_p),
        ("merged_decoder", ctypes.c_char_p),
    ]


class SherpaOnnxOfflineTdnnModelConfig(ctypes.Structure):
    _fields_ = [("model", ctypes.c_char_p)]


class SherpaOnnxOfflineSenseVoiceModelConfig(ctypes.Structure):
    _fields_ = [
        ("model", ctypes.c_char_p),
        ("language", ctypes.c_char_p),
        ("use_itn", ctypes.c_int32),
    ]


class SherpaOnnxOfflineDolphinModelConfig(ctypes.Structure):
    _fields_ = [("model", ctypes.c_char_p)]


class SherpaOnnxOfflineZipformerCtcModelConfig(ctypes.Structure):
    _fields_ = [("model", ctypes.c_char_p)]


class SherpaOnnxOfflineWenetCtcModelConfig(ctypes.Structure):
    _fields_ = [("model", ctypes.c_char_p)]


class SherpaOnnxOfflineOmnilingualAsrCtcModelConfig(ctypes.Structure):
    _fields_ = [("model", ctypes.c_char_p)]


class SherpaOnnxOfflineFunASRNanoModelConfig(ctypes.Structure):
    _fields_ = [
        ("encoder_adaptor", ctypes.c_char_p),
        ("llm", ctypes.c_char_p),
        ("embedding", ctypes.c_char_p),
        ("tokenizer", ctypes.c_char_p),
        ("system_prompt", ctypes.c_char_p),
        ("user_prompt", ctypes.c_char_p),
        ("max_new_tokens", ctypes.c_int32),
        ("temperature", ctypes.c_float),
        ("top_p", ctypes.c_float),
        ("seed", ctypes.c_int32),
        ("language", ctypes.c_char_p),
        ("itn", ctypes.c_int32),
        ("hotwords", ctypes.c_char_p),
    ]


class SherpaOnnxOfflineQwen3ASRModelConfig(ctypes.Structure):
    _fields_ = [
        ("conv_frontend", ctypes.c_char_p),
        ("encoder", ctypes.c_char_p),
        ("decoder", ctypes.c_char_p),
        ("tokenizer", ctypes.c_char_p),
        ("max_total_len", ctypes.c_int32),
        ("max_new_tokens", ctypes.c_int32),
        ("temperature", ctypes.c_float),
        ("top_p", ctypes.c_float),
        ("seed", ctypes.c_int32),
        ("hotwords", ctypes.c_char_p),
    ]


class SherpaOnnxOfflineMedAsrCtcModelConfig(ctypes.Structure):
    _fields_ = [("model", ctypes.c_char_p)]


class SherpaOnnxOfflineModelConfig(ctypes.Structure):
    _fields_ = [
        ("transducer", SherpaOnnxOfflineTransducerModelConfig),
        ("paraformer", SherpaOnnxOfflineParaformerModelConfig),
        ("nemo_ctc", SherpaOnnxOfflineNemoEncDecCtcModelConfig),
        ("whisper", SherpaOnnxOfflineWhisperModelConfig),
        ("tdnn", SherpaOnnxOfflineTdnnModelConfig),
        ("tokens", ctypes.c_char_p),
        ("num_threads", ctypes.c_int32),
        ("debug", ctypes.c_int32),
        ("provider", ctypes.c_char_p),
        ("model_type", ctypes.c_char_p),
        ("modeling_unit", ctypes.c_char_p),
        ("bpe_vocab", ctypes.c_char_p),
        ("telespeech_ctc", ctypes.c_char_p),
        ("sense_voice", SherpaOnnxOfflineSenseVoiceModelConfig),
        ("moonshine", SherpaOnnxOfflineMoonshineModelConfig),
        ("fire_red_asr", SherpaOnnxOfflineFireRedAsrModelConfig),
        ("dolphin", SherpaOnnxOfflineDolphinModelConfig),
        ("zipformer_ctc", SherpaOnnxOfflineZipformerCtcModelConfig),
        ("canary", SherpaOnnxOfflineCanaryModelConfig),
        ("wenet_ctc", SherpaOnnxOfflineWenetCtcModelConfig),
        ("omnilingual", SherpaOnnxOfflineOmnilingualAsrCtcModelConfig),
        ("medasr", SherpaOnnxOfflineMedAsrCtcModelConfig),
        ("funasr_nano", SherpaOnnxOfflineFunASRNanoModelConfig),
        ("fire_red_asr_ctc", SherpaOnnxOfflineFireRedAsrCtcModelConfig),
        ("qwen3_asr", SherpaOnnxOfflineQwen3ASRModelConfig),
        ("cohere_transcribe", SherpaOnnxOfflineCohereTranscribeModelConfig),
    ]


class SherpaOnnxOfflineLmConfig(ctypes.Structure):
    _fields_ = [("model", ctypes.c_char_p), ("scale", ctypes.c_float)]


class SherpaOnnxHomophoneReplacerConfig(ctypes.Structure):
    _fields_ = [
        ("dict_dir", ctypes.c_char_p),
        ("lexicon", ctypes.c_char_p),
        ("rule_fsts", ctypes.c_char_p),
    ]


class SherpaOnnxOfflineRecognizerConfig(ctypes.Structure):
    _fields_ = [
        ("feat_config", SherpaOnnxFeatureConfig),
        ("model_config", SherpaOnnxOfflineModelConfig),
        ("lm_config", SherpaOnnxOfflineLmConfig),
        ("decoding_method", ctypes.c_char_p),
        ("max_active_paths", ctypes.c_int32),
        ("hotwords_file", ctypes.c_char_p),
        ("hotwords_score", ctypes.c_float),
        ("rule_fsts", ctypes.c_char_p),
        ("rule_fars", ctypes.c_char_p),
        ("blank_penalty", ctypes.c_float),
        ("hr", SherpaOnnxHomophoneReplacerConfig),
    ]


class Wave(ctypes.Structure):
    _fields_ = [
        ("samples", ctypes.POINTER(ctypes.c_float)),
        ("sample_rate", ctypes.c_int32),
        ("num_samples", ctypes.c_int32),
    ]


def encoded_path(value: str) -> bytes:
    return os.fsencode(os.path.abspath(os.path.expanduser(value)))


class AsrEngine:
    def __init__(self, library: str, model: str, tokens: str, num_threads: int) -> None:
        self.lib = ctypes.CDLL(os.path.expanduser(library))
        self.lock = threading.Lock()
        self._bind()

        config = SherpaOnnxOfflineRecognizerConfig()
        config.feat_config.sample_rate = 16000
        config.feat_config.feature_dim = 80
        config.model_config.paraformer.model = encoded_path(model)
        config.model_config.tokens = encoded_path(tokens)
        config.model_config.num_threads = num_threads
        config.model_config.debug = 0
        config.model_config.provider = b"cpu"
        config.model_config.model_type = b"paraformer"
        config.decoding_method = b"greedy_search"
        config.max_active_paths = 4

        self.recognizer = self.lib.SherpaOnnxCreateOfflineRecognizer(ctypes.byref(config))
        if not self.recognizer:
            raise RuntimeError("failed to initialize the Paraformer recognizer")

    def _bind(self) -> None:
        self.lib.SherpaOnnxCreateOfflineRecognizer.argtypes = [
            ctypes.POINTER(SherpaOnnxOfflineRecognizerConfig)
        ]
        self.lib.SherpaOnnxCreateOfflineRecognizer.restype = ctypes.c_void_p
        self.lib.SherpaOnnxDestroyOfflineRecognizer.argtypes = [ctypes.c_void_p]
        self.lib.SherpaOnnxCreateOfflineStream.argtypes = [ctypes.c_void_p]
        self.lib.SherpaOnnxCreateOfflineStream.restype = ctypes.c_void_p
        self.lib.SherpaOnnxDestroyOfflineStream.argtypes = [ctypes.c_void_p]
        self.lib.SherpaOnnxAcceptWaveformOffline.argtypes = [
            ctypes.c_void_p,
            ctypes.c_int32,
            ctypes.POINTER(ctypes.c_float),
            ctypes.c_int32,
        ]
        self.lib.SherpaOnnxDecodeOfflineStream.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self.lib.SherpaOnnxGetOfflineStreamResultAsJson.argtypes = [ctypes.c_void_p]
        self.lib.SherpaOnnxGetOfflineStreamResultAsJson.restype = ctypes.c_void_p
        self.lib.SherpaOnnxDestroyOfflineStreamResultJson.argtypes = [ctypes.c_void_p]
        self.lib.SherpaOnnxReadWave.argtypes = [ctypes.c_char_p]
        self.lib.SherpaOnnxReadWave.restype = ctypes.POINTER(Wave)
        self.lib.SherpaOnnxFreeWave.argtypes = [ctypes.POINTER(Wave)]

    def transcribe(self, wav_path: str) -> str:
        with self.lock:
            wave = self.lib.SherpaOnnxReadWave(encoded_path(wav_path))
            if not wave:
                raise ValueError("audio must be a mono, 16-bit PCM WAV file")
            stream = None
            result_json = None
            try:
                stream = self.lib.SherpaOnnxCreateOfflineStream(self.recognizer)
                if not stream:
                    raise RuntimeError("failed to create recognition stream")
                self.lib.SherpaOnnxAcceptWaveformOffline(
                    stream,
                    wave.contents.sample_rate,
                    wave.contents.samples,
                    wave.contents.num_samples,
                )
                self.lib.SherpaOnnxDecodeOfflineStream(self.recognizer, stream)
                result_json = self.lib.SherpaOnnxGetOfflineStreamResultAsJson(stream)
                if not result_json:
                    raise RuntimeError("recognizer returned no result")
                payload = json.loads(ctypes.string_at(result_json).decode("utf-8", "replace"))
                return str(payload.get("text", "")).strip()
            finally:
                if result_json:
                    self.lib.SherpaOnnxDestroyOfflineStreamResultJson(result_json)
                if stream:
                    self.lib.SherpaOnnxDestroyOfflineStream(stream)
                self.lib.SherpaOnnxFreeWave(wave)

    def close(self) -> None:
        if self.recognizer:
            self.lib.SherpaOnnxDestroyOfflineRecognizer(self.recognizer)
            self.recognizer = None


ENGINE: AsrEngine
BEARER_TOKEN = ""
MAX_AUDIO_BYTES = 25 * 1024 * 1024


class Handler(BaseHTTPRequestHandler):
    server_version = "KylinBotASR/1.0"

    def send_json(self, status: int, payload: dict[str, object]) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if self.path == "/health":
            self.send_json(200, {"status": "ready"})
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
            audio = next(
                (
                    part.get_payload(decode=True)
                    for part in message.iter_parts()
                    if part.get_param("name", header="content-disposition") == "file"
                ),
                None,
            )
            if audio is None:
                raise ValueError("multipart field 'file' is required")
            if len(audio) > MAX_AUDIO_BYTES:
                raise ValueError("audio file is too large")
            with tempfile.NamedTemporaryFile(suffix=".wav") as wav:
                wav.write(audio)
                wav.flush()
                text = ENGINE.transcribe(wav.name)
            self.send_json(200, {"text": text})
        except Exception as error:
            self.send_json(400, {"error": str(error)})

    def log_message(self, fmt: str, *args: object) -> None:
        print(f"{self.client_address[0]} {fmt % args}", flush=True)


def main() -> None:
    global ENGINE, BEARER_TOKEN, MAX_AUDIO_BYTES
    library = os.environ["KYLINBOT_ASR_LIBRARY"]
    model_dir = pathlib.Path(os.path.expanduser(os.environ["KYLINBOT_ASR_MODEL_DIR"]))
    BEARER_TOKEN = os.environ["KYLINBOT_ASR_TOKEN"]
    MAX_AUDIO_BYTES = int(os.environ.get("KYLINBOT_ASR_MAX_BYTES", str(MAX_AUDIO_BYTES)))
    threads = int(os.environ.get("KYLINBOT_ASR_THREADS", "4"))
    port = int(os.environ.get("KYLINBOT_ASR_PORT", "18081"))
    ENGINE = AsrEngine(
        library,
        str(model_dir / "model.int8.onnx"),
        str(model_dir / "tokens.txt"),
        threads,
    )
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print(f"KylinBot ASR ready on http://127.0.0.1:{port}", flush=True)
    try:
        server.serve_forever()
    finally:
        server.server_close()
        ENGINE.close()


if __name__ == "__main__":
    main()
