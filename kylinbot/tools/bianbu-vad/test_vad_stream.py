"""Tests pre-existing synthesized fixtures only; never opens the microphone."""
import array
import math
import pathlib
import random
import select
import struct
import subprocess
import sys
import time
import wave

binary, model, fixture = sys.argv[1:4]
with wave.open(fixture, 'rb') as wav:
    rate, channels = wav.getframerate(), wav.getnchannels()
    assert wav.getsampwidth() == 2
    pcm = array.array('h', wav.readframes(wav.getnframes()))
speech = [sum(pcm[i:i+channels]) / (32768 * channels) for i in range(0, len(pcm), channels)]
worker = subprocess.Popen([binary, model, str(rate)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0)
poller = select.poll()
poller.register(worker.stdout, select.POLLIN | select.POLLHUP)

def line():
    deadline = time.monotonic() + 20
    result = bytearray()
    while not result.endswith(b'\n'):
        if time.monotonic() > deadline or not poller.poll(1000):
            if time.monotonic() > deadline:
                raise TimeoutError('VAD reply timeout')
            continue
        char = worker.stdout.read(1)
        if not char:
            raise RuntimeError('VAD worker exited early')
        result.extend(char)
        if len(result) > 64:
            raise ValueError('VAD reply too long')
    return bytes(result).strip()

def test(name, samples):
    worker.stdin.write(struct.pack('<I', 0))
    assert line() == b'0'
    hits = 0
    frames = 0
    frame_size = max(1, rate // 50)
    started = time.monotonic()
    for offset in range(0, len(samples), frame_size):
        frame = samples[offset:offset + frame_size]
        packet = struct.pack('<I', len(frame)) + struct.pack('<%df' % len(frame), *frame)
        worker.stdin.write(packet)
        result = line()
        assert result in (b'0', b'1'), result
        hits += result == b'1'
        frames += 1
    result = {'case': name, 'frames': frames, 'speech_frames': hits,
              'audio_secs': round(len(samples) / rate, 3),
              'processing_secs': round(time.monotonic() - started, 3)}
    print(result, flush=True)
    return hits

try:
    assert line() == b'READY'
    assert test('silence', [0.0] * (rate * 2)) == 0
    assert test('synthesized_speech', speech + [0.0] * rate) > 5
    assert test('quiet_synthesized_speech', [s * .1 for s in speech] + [0.0] * rate) > 5
    rng = random.Random(4)
    noise_hits = test('white_noise', [rng.uniform(-.05, .05) for _ in range(rate * 2)])
    tone_hits = test('440hz_tone', [.06 * math.sin(2 * math.pi * 440 * i / rate) for i in range(rate * 2)])
    assert noise_hits == 0, 'loud white noise incorrectly classified as speech'
    assert tone_hits == 0, 'loud pure tone incorrectly classified as speech'
    worker.stdin.close()
    assert worker.wait(timeout=5) == 0
    print('PASS: silence, speech, quiet speech, noise, tone, reset, clean EOF')
finally:
    if worker.poll() is None:
        worker.terminate()
        try: worker.wait(timeout=3)
        except subprocess.TimeoutExpired:
            worker.kill()
            worker.wait()
