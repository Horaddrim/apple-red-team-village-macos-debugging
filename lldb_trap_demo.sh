#!/bin/bash

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="${2:-target-hardened}"
PID="$1"

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
BASE=$(vmmap "$PID" 2>/dev/null | awk -v p="/$BIN\$" '$1=="__TEXT" && $NF ~ p {split($2,a,"-"); print a[1]; exit}')

if [ -n "$BASE" ]; then
    ADDR=$(printf '0x%x' $(( 0x$BASE + OFFSET )))
else
    ADDR=0x100000000
fi

echo "target:  $BIN (pid $PID)"
echo "tool:    mach_vm_adventures read $PID $ADDR 40"
echo
echo "at the breakpoint:"
echo "  si                 step one instruction (mov x16, #-45)"
echo "  si                 step again (svc #0x80 - into the kernel)"
echo "  finish             return to acquire_task"
echo "  register read x0   0 = KERN_SUCCESS, 5 = KERN_FAILURE"
echo

exec lldb \
    -o "breakpoint set --name task_for_pid" \
    -o "run" \
    -- "$DIR/mach_vm_adventures" read "$PID" "$ADDR" 40
