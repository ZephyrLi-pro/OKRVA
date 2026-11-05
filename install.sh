#!/bin/sh
# Run as the logged-in desktop user; privileged operations use sudo explicitly.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$root/deploy/installer/install.py" "$@"
