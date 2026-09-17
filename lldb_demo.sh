#!/bin/sh

DIR="$(cd "$(dirname "$0")" && pwd)"
PASS="${1:-amindneedsbookslikeaswordneedsawhetstone}"

[ -x "$DIR/target" ] || { echo "target missing - run ./build.sh once" >&2; exit 1; }

exec lldb \
    -o "breakpoint set --name detect_debugger" \
    -o "run" \
    -- "$DIR/target" "$PASS"
