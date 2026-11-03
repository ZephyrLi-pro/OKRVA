import importlib.util
import io
import json
import subprocess
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
import wave
from pathlib import Path
from unittest.mock import Mock, patch

script_dir = Path(__file__).resolve().parents[2] / "deploy"
if not (script_dir / "tts_server.py").is_file():
    script_dir = Path(__file__).resolve().parents[1] / "scripts"
spec = importlib.util.spec_from_file_location("tts_server", script_dir / "tts_server.py")
tts = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tts)
native_spec = importlib.util.spec_from_file_location("tts_native", script_dir / "tts_native.py")
native = importlib.util.module_from_spec(native_spec)
native_spec.loader.exec_module(native)


def wav_bytes(width=2, frames=80):
    buffer = io.BytesIO()
    with wave.open(buffer, "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(width)
        audio.setframerate(8000)
        audio.writeframes(b"\x01\x00" * frames if width == 2 else b"\x01" * frames)
    return buffer.getvalue()


class RequestTests(unittest.TestCase):
    def parse(self, **fields):
        return tts.parse_request(json.dumps({"input": "你好。", **fields}).encode())

    def test_defaults(self):
        self.assertEqual(self.parse(), ("你好。", 33, 1.0))

    def test_voice_range(self):
        self.assertEqual(self.parse(voice="zh-173")[1], 173)
        for value in ("zh-174", "zh--1", "other", 33):
            with self.assertRaises(ValueError):
                self.parse(voice=value)

    def test_speed(self):
        self.assertEqual(self.parse(speed=1.25)[2], 1.25)
        for value in (True, 0.0, 3.0, float("nan"), float("inf"), "1"):
            with self.assertRaises(ValueError):
                self.parse(speed=value)

    def test_text_limits(self):
        for value in ("", " ", "a" * 2001, "a\x00b", None):
            with self.assertRaises(ValueError):
                self.parse(input=value)

    def test_formats_and_models(self):
        for fields in ({"model": "other"}, {"response_format": "mp3"}):
            with self.assertRaises(ValueError):
                self.parse(**fields)

    def test_melo_profile(self):
        body = {"input": "我是KylinBot，一个本地系统助手。"}
        self.assertEqual(tts.parse_request(json.dumps(body).encode(), "melo-zh-en"),
                         (body["input"], 0, 1.0))
        for voice in ("zh-0", "alloy"):
            self.assertEqual(tts.parse_request(json.dumps({**body, "voice": voice}).encode(), "melo-zh-en")[1], 0)
        for model in ("vits-zh", "melo-zh-en", "vits-melo-tts-zh_en"):
            self.assertEqual(tts.parse_request(json.dumps({**body, "model": model}).encode(), "melo-zh-en")[0], body["input"])
        for fields in ({"voice": "zh-33"}, {"voice": "zh-1"}, {"model": "vits-icefall-zh-aishell3"}):
            with self.assertRaises(ValueError):
                tts.parse_request(json.dumps({**body, **fields}).encode(), "melo-zh-en")

    def test_project_lexicon(self):
        tokens = "_ 0\nk 1\nih 2\nl 3\nn 4\nb 5\naa 6\nt 7\n"
        entry = "kylinbot k ih l ih n b aa t 7 9 7 8 7 7 9 7\n"
        self.assertEqual(tts.validate_melo_lexicon("# English project name\n" + entry, tokens), entry)
        for extra in ("", "kylinbot k 7 7", "kylinbot unknown 7", "kylinbot k -1",
                      "kylinbot k 51", "kylinbot k NaN", "kylinbot k ７", entry + entry,
                      "麒麟 k 7"):
            with self.assertRaises(ValueError):
                tts.validate_melo_lexicon(extra, tokens)

    def test_combined_lexicon_preserves_model(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            binary = folder / "tts"
            binary.touch()
            for name in ("model.onnx", "phone.fst", "date.fst", "number.fst"):
                (folder / name).touch()
            (folder / "tokens.txt").write_text("k 0\nih 1\n", encoding="utf-8")
            base = "hello k ih 7 9\n"
            (folder / "lexicon.txt").write_text(base, encoding="utf-8")
            extra = folder / "project.txt"
            extra.write_text("kylin k ih 7 9\n", encoding="utf-8")
            server = tts.TtsServer(binary, folder, 0, 2, "melo-zh-en", extra)
            merged = server.lexicon
            try:
                self.assertEqual(merged.read_text(encoding="utf-8"), "kylin k ih 7 9\n" + base)
                self.assertEqual((folder / "lexicon.txt").read_text(encoding="utf-8"), base)
                self.assertEqual(server.default_sid, 0)
            finally:
                server.server_close()
            self.assertFalse(merged.exists())

    def test_lfs_pointer_is_not_a_model(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            for name in ("tts", "tokens.txt", "lexicon.txt", "phone.fst", "date.fst", "number.fst"):
                (folder / name).touch()
            (folder / "model.int8.onnx").write_text("version https://git-lfs.github.com/spec/v1\n")
            with self.assertRaisesRegex(ValueError, "Git LFS"):
                tts.TtsServer(folder / "tts", folder, 0, 2, "melo-zh-en", model_file="model.int8.onnx")
            with self.assertRaisesRegex(ValueError, "filename"):
                tts.TtsServer(folder / "tts", folder, 0, 2, model_file="../escape.onnx")

    def test_wav(self):
        self.assertEqual(tts.validate_wav(wav_bytes()), (8000, 0.01))
        for data in (wav_bytes(width=1), wav_bytes(frames=0), wav_bytes()[:-2]):
            with self.assertRaises(ValueError):
                tts.validate_wav(data)


class HttpTests(unittest.TestCase):
    def setUp(self):
        with patch.object(Path, "is_file", return_value=True):
            self.server = tts.TtsServer(Path("tts"), Path("model"), 0, 2)
        self.server.ready = True
        self.worker = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.worker.start()
        self.url = "http://127.0.0.1:" + str(self.server.server_port)

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.worker.join(2)

    def post(self, value):
        request = urllib.request.Request(self.url + "/v1/audio/speech", data=json.dumps(value).encode(), headers={"Content-Type": "application/json"})
        return urllib.request.urlopen(request, timeout=5)

    def test_health(self):
        with urllib.request.urlopen(self.url + "/health") as response:
            self.assertEqual(json.load(response)["status"], "ready")

    def test_melo_health_and_http(self):
        self.server.profile = "melo-zh-en"
        self.server.model_name = "vits-melo-tts-zh_en"
        with urllib.request.urlopen(self.url + "/health") as response:
            result = json.load(response)
            self.assertEqual(result["model"], "vits-melo-tts-zh_en")
            self.assertEqual(result["languages"], ["zh", "en"])
        text = "我是KylinBot，一个本地系统助手。Hello!"
        with patch.object(self.server, "synthesize", return_value=wav_bytes()) as synthesize:
            with self.post({"input": text, "voice": "zh-0", "model": "vits-zh"}) as response:
                self.assertEqual(response.status, 200)
            synthesize.assert_called_once_with(text, 0, 1.0)
        with self.assertRaises(urllib.error.HTTPError) as caught:
            self.post({"input": text, "voice": "zh-33"})
        self.assertEqual(caught.exception.code, 400)

    def test_audio(self):
        with patch.object(self.server, "synthesize", return_value=wav_bytes()):
            with self.post({"input": "测试。"}) as response:
                self.assertEqual(response.headers["Content-Type"], "audio/wav")
                self.assertEqual(response.read(), wav_bytes())

    def test_bad_request(self):
        with self.assertRaises(urllib.error.HTTPError) as caught:
            self.post({"input": ""})
        self.assertEqual(caught.exception.code, 400)

    def test_busy(self):
        self.server.gate.acquire()
        try:
            with self.assertRaises(urllib.error.HTTPError) as caught:
                self.post({"input": "测试。"})
            self.assertEqual(caught.exception.code, 429)
        finally:
            self.server.gate.release()

    def test_warming_rejects_synthesis(self):
        self.server.ready = False
        with patch.object(self.server, "synthesize") as synthesize:
            with self.assertRaises(urllib.error.HTTPError) as caught:
                self.post({"input": "测试。"})
            self.assertEqual(caught.exception.code, 503)
            synthesize.assert_not_called()

    def test_native_crash_exits_http_loop(self):
        engine = Mock()
        engine.is_alive.return_value = False
        self.server.native_engine = engine
        with patch.object(self.server, "synthesize", side_effect=subprocess.TimeoutExpired("tts", 120)):
            with self.assertRaises(urllib.error.HTTPError) as caught:
                self.post({"input": "测试。"})
            self.assertEqual(caught.exception.code, 504)
        self.worker.join(2)
        self.assertFalse(self.worker.is_alive())
        self.assertFalse(self.server.ready)
        self.assertTrue(self.server.failed)

    def test_timeout_releases_gate(self):
        with patch.object(self.server, "synthesize", side_effect=subprocess.TimeoutExpired("tts", 120)):
            with self.assertRaises(urllib.error.HTTPError) as caught:
                self.post({"input": "测试。"})
            self.assertEqual(caught.exception.code, 504)
        self.assertTrue(self.server.gate.acquire(timeout=1))
        self.server.gate.release()


class NativeTests(unittest.TestCase):
    def setUp(self):
        self.engine = native.NativeEngine.__new__(native.NativeEngine)
        self.engine.connection = Mock()
        self.engine.process = Mock()

    def test_model_reused_and_audio_bounded(self):
        self.engine.connection.poll.return_value = True
        self.engine.connection.recv_bytes.return_value = wav_bytes()
        for _ in range(2):
            self.assertEqual(self.engine.synthesize("KylinBot，你好。", 0, 1.0), wav_bytes())
        self.assertEqual(self.engine.connection.send.call_count, 2)
        self.engine.connection.send.assert_called_with(("KylinBot，你好。", 0, 1.0))
        self.engine.connection.recv_bytes.assert_called_with(tts.MAX_AUDIO)
        self.engine.process.start.assert_not_called()

    def test_hard_timeout_kills_hung_child(self):
        self.engine.connection.poll.return_value = False
        self.engine.process.is_alive.return_value = True
        with self.assertRaises(subprocess.TimeoutExpired):
            self.engine.synthesize("Hello", 0, 1.0, timeout=0.01)
        self.engine.process.terminate.assert_called_once()
        self.engine.process.kill.assert_called_once()
        self.engine.connection.close.assert_called_once()

    def test_child_disconnect(self):
        self.engine.connection.send.side_effect = BrokenPipeError
        self.engine.process.is_alive.return_value = False
        with self.assertRaises(RuntimeError):
            self.engine.synthesize("Hello", 0, 1.0)
        self.engine.connection.close.assert_called_once()


if __name__ == "__main__":
    unittest.main()
