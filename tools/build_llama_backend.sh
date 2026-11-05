#!/bin/sh
# Build from source; no cached objects, package replacement, model download or service restart.
set -eu
bundle_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_dir=${1:?usage: build_llama_backend.sh /path/to/pinned-vendor-source /path/to/new-build}
build_dir=${2:?usage: build_llama_backend.sh /path/to/pinned-vendor-source /path/to/new-build}
python3 "$bundle_dir/tools/apply_backend_changes.py" llama "$source_dir"
cmake -S "$source_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 \
    -DGGML_CPU_RISCV64_SPACEMIT=ON -DGGML_CPU_REPACK=ON -DGGML_RV_ZBA=ON \
    -DLLAMA_OPENSSL=OFF -DLLAMA_CURL=OFF -DLLAMA_SERVER_SMT_VISION=OFF \
    -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF -DCMAKE_SKIP_BUILD_RPATH=ON
cmake --build "$build_dir" --parallel "${BUILD_JOBS:-2}" --target ggml
printf 'Built private GGML libraries in %s/bin; nothing installed.\n' "$build_dir"
