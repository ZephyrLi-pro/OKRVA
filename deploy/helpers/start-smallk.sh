#!/bin/sh
# Keep the actual UI process owned by its lifecycle service. Secondary
# invocations only activate the existing QtSingleApplication instance.
set -eu
systemctl --user start okrva-aiassistant.service
attempt=0
until busctl --user --no-pager status org.ukui.kylin_aiassistant >/dev/null 2>&1; do
    attempt=$((attempt + 1))
    if [ "$attempt" -ge 100 ]; then
        echo 'SmallK did not become ready' >&2
        exit 1
    fi
    sleep 0.1
done
exec /usr/bin/kylin-aiassistant "$@"
