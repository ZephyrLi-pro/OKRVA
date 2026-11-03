#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Use installed ORT/espeak-ng development files; never rebuild the system ORT.
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=${RVAI_MATCHA_BUILD_DIR:-"$script_dir/build-matcha"}
cmake -S "$script_dir/third_party/spacemit-tts" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release -DTTS_USE_FST=ON -DUSE_SPACEMIT_EP=OFF \
  -DBUILD_SIMPLE_DEMO=OFF -DBUILD_STREAM_DEMO=OFF -DBUILD_PYTHON_BINDINGS=OFF \
  -DRVAI_MATCHA_BRIDGE_SOURCE="$script_dir/tts_matcha_bridge.cpp" "$@"
cmake --build "$build_dir" --target kylinbot_matcha --parallel "${BUILD_JOBS:-2}"
printf 'Matcha bridge: %s/lib/libkylinbot-matcha.so\n' "$build_dir"
