#!/bin/sh
# Build only our tiny C wrapper against the existing sherpa-onnx 1.13.2 runtime.
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
runtime_dir=${1:?usage: build_tts_native.sh /absolute/path/to/sherpa-v1.13.2}
header_dir="$script_dir/third_party/sherpa-onnx-1.13.2"
test -f "$runtime_dir/lib/libsherpa-onnx-c-api.so"
test -f "$header_dir/c-api.h"
actual_hash=$(sha256sum "$header_dir/c-api.h")
case "$actual_hash" in
    437b1279047877167d8fadc74a60d47f3df514d703fdac1c1b6851da9bc2fdb4\ *) ;;
    *) echo "sherpa-onnx C header differs from the pinned 1.13.2 header" >&2; exit 1 ;;
esac
cc -Wall -Wextra -Werror -O2 -shared -fPIC -I"$header_dir" \
    "$script_dir/tts_native_bridge.c" -L"$runtime_dir/lib" \
    -Wl,-rpath,"$runtime_dir/lib" -lsherpa-onnx-c-api \
    -o "$script_dir/libkylinbot-tts.so"
