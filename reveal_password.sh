#!/bin/bash

DIR="$(cd "$(dirname "$0")" && pwd)"
PID="$1"
BIN="${2:-target}"
KEY="azeroth"
SIZE=40

[ -x "$DIR/mach_vm_adventures" ] || { echo "mach_vm_adventures missing - run ./build.sh once" >&2; exit 1; }

if [ -z "$PID" ]; then
    PID=$(pgrep -n -x "$BIN")
fi

if [ -z "$PID" ]; then
    echo "no running $BIN found" >&2
    echo "start one with:  ./$BIN amindneedsbookslikeaswordneedsawhetstone hold" >&2
    exit 1
fi

OFFSET=$(otool -l "$DIR/$BIN" | awk '/sectname __const/{f=1} f && /^ *offset/{print $2; exit}')

if [ -z "$OFFSET" ]; then
    echo "could not locate __TEXT,__const in $BIN" >&2
    exit 1
fi

BASE=$(vmmap "$PID" 2>/dev/null | awk -v p="/$BIN\$" '$1=="__TEXT" && $NF ~ p {split($2,a,"-"); print a[1]; exit}')

if [ -z "$BASE" ]; then
    echo "could not read __TEXT base for pid $PID" >&2
    exit 1
fi

ADDR=$(printf '0x%x' $(( 0x$BASE + OFFSET )))

echo "--- static ---"
printf '  binary          %s\n' "$BIN"
printf '  __const offset  0x%x\n' "$OFFSET"
echo "--- live ---"
printf '  pid             %s\n' "$PID"
printf '  __TEXT base     0x%s\n' "$BASE"
printf '  computed addr   %s\n' "$ADDR"
echo
echo "--- command ---"
echo "  ./mach_vm_adventures read $PID $ADDR $SIZE"
echo

read -n 1 -s -r -p "press any key to run it "
echo
echo

OUT=$("$DIR/mach_vm_adventures" read "$PID" "$ADDR" "$SIZE" 2>&1)
STATUS=$?
echo "$OUT"

if [ $STATUS -ne 0 ]; then
    exit 1
fi

HEX=$(echo "$OUT" | sed -n '2p')

PLAIN=""
i=0
for b in $HEX; do
    kc="${KEY:$((i % ${#KEY})):1}"
    printf -v kv '%d' "'$kc"
    v=$(( 0x$b ^ kv ))
    PLAIN+=$(printf "\\$(printf '%03o' "$v")")
    i=$((i + 1))
done

echo
echo "--- xor with \"$KEY\" ---"
echo "  $PLAIN"
