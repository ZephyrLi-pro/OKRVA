#!/bin/sh
set -eu
release_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$release_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SPACEMIT_Q4K_CPU_DECODE=1
export SPACEMIT_DISABLE_TCM=0
export SPACEMIT_K1_TCM_SPLIT=1
exec /usr/bin/taskset -c 0-3 /usr/bin/llama-server "$@"
