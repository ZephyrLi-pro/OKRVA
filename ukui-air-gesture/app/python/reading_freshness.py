"""Bound result latency without dropping a normal full palm-detection pass."""
import math

# On this K1 a full palm+pose pass takes about .8 s even with the LLM idle.
# LatestFrame supplies the newest capture, so this is mostly inference time,
# not accumulated camera/gesture queue time. Results beyond 1 s remain invalid.
MAX_RESULT_AGE_SECONDS=1.

def result_is_fresh(age):
    return math.isfinite(age) and 0<=age<=MAX_RESULT_AGE_SECONDS
