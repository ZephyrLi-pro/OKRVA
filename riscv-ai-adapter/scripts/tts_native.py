"""Resident sherpa/Matcha model in an isolated process with a hard deadline.

The native bridges own model structures, avoiding ctypes ABI guesses.
Only this module's child loads the library; a hung inference can be terminated.
"""
from __future__ import annotations

import ctypes
import multiprocessing
import os
import subprocess
import time
from pathlib import Path

MAX_AUDIO = 32 * 1024 * 1024


def engine_child(connection, bridge: str, model_dir: str, lexicon: str, threads: int,
                 model_file: str, backend: str = "sherpa", cancellation=None) -> None:
    # Native libraries can include the user's text in diagnostics. Never send
    # their stdout/stderr to the service journal or persist them on disk.
    with open(os.devnull, "wb") as sink:
        os.dup2(sink.fileno(), 1)
        os.dup2(sink.fileno(), 2)
    handle = None
    library = None
    try:
        library = ctypes.CDLL(bridge)
        if backend == "matcha":
            library.rv_tts_create_matcha.argtypes = [ctypes.c_char_p, ctypes.c_int32]
            library.rv_tts_create_matcha.restype = ctypes.c_void_p
            library.rv_tts_abi_version.restype = ctypes.c_int32
            if library.rv_tts_abi_version() != 1:
                raise RuntimeError("incompatible Matcha bridge")
        else:
            library.rv_tts_create.argtypes = [ctypes.c_char_p] * 4 + [ctypes.c_int32]
            library.rv_tts_create.restype = ctypes.c_void_p
        library.rv_tts_generate_wav.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
                                               ctypes.c_int32, ctypes.c_float,
                                               ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_int64)]
        library.rv_tts_generate_wav.restype = ctypes.c_int32
        library.rv_tts_free_audio.argtypes = [ctypes.c_void_p]
        library.rv_tts_free_audio.restype = None
        library.rv_tts_destroy.argtypes = [ctypes.c_void_p]
        library.rv_tts_destroy.restype = None
        for name in ("rv_tts_sample_rate", "rv_tts_num_speakers"):
            function = getattr(library, name)
            function.argtypes = [ctypes.c_void_p]
            function.restype = ctypes.c_int32
        folder = Path(model_dir)
        rules = ",".join(str(folder / name) for name in ("phone.fst", "date.fst", "number.fst"))
        if backend == "matcha":
            handle = library.rv_tts_create_matcha(str(folder).encode(), threads)
            cancel_type = ctypes.CFUNCTYPE(ctypes.c_int32)
            cancel_callback = cancel_type(lambda: int(cancellation.is_set()))
            library.rv_tts_generate_wav_cancelable.argtypes = library.rv_tts_generate_wav.argtypes + [cancel_type]
            library.rv_tts_generate_wav_cancelable.restype = ctypes.c_int32
        else:
            handle = library.rv_tts_create(str(folder / model_file).encode(),
                                           str(folder / "tokens.txt").encode(), lexicon.encode(),
                                           rules.encode(), threads)
        if not handle:
            connection.send({"error": "native TTS initialization failed"})
            return
        connection.send({"sample_rate": library.rv_tts_sample_rate(handle),
                         "speakers": library.rv_tts_num_speakers(handle)})
        while True:
            request = connection.recv()
            if request is None:
                break
            text, sid, speed = request
            buffer, size = ctypes.c_void_p(), ctypes.c_int64()
            try:
                if backend == "matcha":
                    ok = library.rv_tts_generate_wav_cancelable(handle, text.encode(), sid, speed,
                        ctypes.byref(buffer), ctypes.byref(size), cancel_callback)
                else:
                    ok = library.rv_tts_generate_wav(handle, text.encode(), sid, speed,
                                                     ctypes.byref(buffer), ctypes.byref(size))
                if ok != 1 or not buffer.value or not 0 < size.value <= MAX_AUDIO:
                    connection.send_bytes(b"")
                else:
                    connection.send_bytes(ctypes.string_at(buffer, size.value))
            finally:
                library.rv_tts_free_audio(buffer)
    except (EOFError, BrokenPipeError):
        pass
    except Exception:
        # Keep diagnostics generic: exceptions may contain the request text.
        try:
            connection.send({"error": "native TTS child failed"})
        except (OSError, EOFError):
            pass
    finally:
        if handle and library:
            library.rv_tts_destroy(handle)
        connection.close()


class NativeEngine:
    def __init__(self, bridge: Path, model_dir: Path, lexicon: Path, threads: int,
                 startup_timeout: float = 180, model_file: str = "model.onnx",
                 backend: str = "sherpa"):
        if backend not in ("sherpa", "matcha"):
            raise ValueError("unsupported native backend")
        self.backend = backend
        context = multiprocessing.get_context("spawn")
        self.cancellation = context.Event() if backend == "matcha" else None
        self.connection, child = context.Pipe()
        self.process = context.Process(target=engine_child,
                                       args=(child, str(bridge), str(model_dir), str(lexicon), threads, model_file,
                                             backend, self.cancellation),
                                       daemon=True)
        self.process.start()
        child.close()
        try:
            if not self.connection.poll(startup_timeout):
                raise TimeoutError("native TTS initialization timed out")
            result = self.connection.recv()
            if "error" in result:
                raise RuntimeError(result["error"])
            self.sample_rate, self.speakers = result["sample_rate"], result["speakers"]
        except BaseException:
            self.close()
            raise

    def synthesize(self, text: str, sid: int, speed: float, timeout: float = 120, cancelled=None) -> bytes:
        cancellable = getattr(self, "backend", "sherpa") == "matcha"
        if cancellable:
            self.cancellation.clear()
        interrupted = False
        try:
            self.connection.send((text, sid, speed))
            if cancellable:
                deadline = time.monotonic() + timeout
                while not self.connection.poll(min(0.1, max(0, deadline-time.monotonic()))):
                    if not interrupted and cancelled is not None and cancelled():
                        self.cancellation.set()
                        interrupted = True
                        # Let the current bounded clause finish, then reuse the model.
                        deadline = min(deadline, time.monotonic() + 15)
                    if time.monotonic() >= deadline:
                        self.close()
                        raise subprocess.TimeoutExpired("native TTS", timeout)
            elif not self.connection.poll(timeout):
                self.close()
                raise subprocess.TimeoutExpired("native TTS", timeout)
            data = self.connection.recv_bytes(MAX_AUDIO)
        except (OSError, EOFError):
            self.close()
            raise RuntimeError("native TTS child stopped") from None
        if interrupted or (cancellable and cancelled is not None and cancelled()):
            raise InterruptedError("speech client disconnected")
        if not data:
            raise RuntimeError("native TTS produced no audio")
        return data

    def is_alive(self) -> bool:
        return self.process.is_alive()

    def close(self) -> None:
        if self.process.is_alive():
            self.process.terminate()
            self.process.join(3)
            if self.process.is_alive():
                self.process.kill()
                self.process.join(3)
        self.connection.close()
