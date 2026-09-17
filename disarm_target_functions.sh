#!/bin/sh

DIR="$(cd "$(dirname "$0")" && pwd)"
DISARM="${DISARM:-$DIR/tools/disarm}"
FUNC="$1"

if [ ! -x "$DISARM" ]; then
    echo "disarm not found at $DISARM (override with DISARM=/path/to/disarm)" >&2
    exit 1
fi

[ -x "$DIR/target" ] || { echo "target missing - run ./build.sh once" >&2; exit 1; }

if [ -z "$FUNC" ]; then
    echo "--- functions in target (disarm -S) ---"
    "$DISARM" -S "$DIR/target" 2>&1 | grep -v 'Slot  *[0-9]' | grep ' T '
    echo
    echo "usage: $(basename "$0") <function>   e.g. $(basename "$0") detect_debugger"
    exit 0
fi

case "$FUNC" in
    _*) SYM="$FUNC" ;;
    *)  SYM="_$FUNC" ;;
esac

echo "--- $SYM (disarm -r) ---"
JCOLOR=1 "$DISARM" -r "$SYM" "$DIR/target" 2>&1 | grep -v 'Slot  *[0-9]'
