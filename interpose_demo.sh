#!/bin/sh

DIR="$(cd "$(dirname "$0")" && pwd)"
PASS="${1:-wrong}"

for f in target interpose.dylib; do
    [ -e "$DIR/$f" ] || { echo "$f missing - run ./build.sh once" >&2; exit 1; }
done

echo "--- without interpose ---"
"$DIR/target" "$PASS"
echo "exit=$?"

echo "--- with interpose ---"
DYLD_INSERT_LIBRARIES="$DIR/interpose.dylib" "$DIR/target" "$PASS"
echo "exit=$?"
