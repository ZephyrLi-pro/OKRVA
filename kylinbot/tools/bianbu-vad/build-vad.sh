#!/usr/bin/env bash
set -Eeuo pipefail
task_dir=$(cd -- "$(dirname -- "$0")" && pwd)
runtime_dir=${1:?usage: build-vad.sh /absolute/path/to/installed-sherpa-runtime}
test -n "$runtime_dir"
test -f "$runtime_dir/lib/libsherpa-onnx-c-api.so"
cc -std=c11 -O2 -Wall -Wextra -Werror -I "$task_dir/include" \
    "$task_dir/kylinbot-vad-stream.c" -L "$runtime_dir/lib" \
    -Wl,-rpath,"$runtime_dir/lib" -lsherpa-onnx-c-api \
    -o "$task_dir/kylinbot-vad-stream"
printf 'VAD helper built using installed sherpa runtime: %s\n' "$runtime_dir"
