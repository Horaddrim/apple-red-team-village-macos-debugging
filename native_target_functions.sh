#!/bin/sh

DIR="$(cd "$(dirname "$0")" && pwd)"
FUNC="$1"

[ -x "$DIR/target" ] || { echo "target missing - run ./build.sh once" >&2; exit 1; }

if [ -z "$FUNC" ]; then
    echo "--- functions in target (nm) ---"
    nm -n "$DIR/target" | grep ' T '
    echo
    echo "--- __TEXT,__text entry points (otool -tV) ---"
    otool -tV "$DIR/target" | grep -E '^_[A-Za-z0-9_]+:$'
    echo
    echo "usage: $(basename "$0") <function>   e.g. $(basename "$0") arthas"
    exit 0
fi

case "$FUNC" in
    _*) SYM="$FUNC" ;;
    *)  SYM="_$FUNC" ;;
esac

echo "--- $SYM (otool -tV) ---"
otool -tV "$DIR/target" | awk -v want="$SYM:" '
    $0 == want { inside = 1; print; next }
    inside && /^_[A-Za-z0-9_]+:$/ { exit }
    inside { print }
'
