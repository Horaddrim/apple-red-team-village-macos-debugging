#!/bin/bash
#
# Sets a breakpoint in a live process with our own tool - no lldb, no ptrace.
# Resolves a symbol to a runtime address the same way reveal_password.sh does,
# then hands it to `hddb break`, which writes brk #0 and waits for the trap on a
# Mach exception port.
#
#   ./breakpoint_demo.sh                       target / check_passphrase
#   ./breakpoint_demo.sh "" target detect_debugger
#   ./breakpoint_demo.sh "" target-hardened    (fails at task_for_pid, on purpose)

DIR="$(cd "$(dirname "$0")" && pwd)"
PID="$1"
BIN="${2:-target}"
SYM="${3:-check_passphrase}"

[ -x "$DIR/hddb" ] || { echo "hddb missing - run ./build.sh once" >&2; exit 1; }

if [ -z "$PID" ]; then
    PID=$(pgrep -n -x "$BIN")
fi

if [ -z "$PID" ]; then
    echo "no running $BIN found" >&2
    echo "start one with:  ./$BIN amindneedsbookslikeaswordneedsawhetstone hold" >&2
    exit 1
fi

# nm gives the symbol's vmaddr; __TEXT's own vmaddr turns it into a file offset.
SYMADDR=$(nm -n "$DIR/$BIN" | awk -v s="_$SYM" '$3 == s { print $1; exit }')
VMADDR=$(otool -l "$DIR/$BIN" | awk '/segname __TEXT$/ { f = 1 } f && /vmaddr/ { print $2; exit }')

if [ -z "$SYMADDR" ]; then
    echo "symbol $SYM not found in $BIN" >&2
    echo "try:  nm -n $BIN | grep ' T '" >&2
    exit 1
fi

BASE=$(vmmap "$PID" 2>/dev/null | awk -v p="/$BIN\$" '$1=="__TEXT" && $NF ~ p {split($2,a,"-"); print a[1]; exit}')

if [ -z "$BASE" ]; then
    echo "could not read __TEXT base for pid $PID" >&2
    exit 1
fi

OFFSET=$(( 0x$SYMADDR - VMADDR ))
ADDR=$(printf '0x%x' $(( 0x$BASE + OFFSET )))

pflag() { ps -p "$1" -o flags= 2>/dev/null | tr -d ' '; }

traced() {
    local f
    f=$(pflag "$1")
    [ -n "$f" ] || { echo "?"; return; }
    if (( 0x$f & 0x800 )); then echo "1"; else echo "0"; fi
}

echo "--- static ---"
printf '  binary          %s\n' "$BIN"
printf '  symbol          %s\n' "$SYM"
printf '  vmaddr          0x%s\n' "$SYMADDR"
printf '  __TEXT vmaddr   %s\n' "$VMADDR"
printf '  file offset     0x%x\n' "$OFFSET"
echo "--- live ---"
printf '  pid             %s\n' "$PID"
printf '  __TEXT base     0x%s\n' "$BASE"
printf '  computed addr   %s\n' "$ADDR"
echo "--- before ---"
printf '  p_flag          0x%s   P_TRACED=%s\n' "$(pflag "$PID")" "$(traced "$PID")"
[ -x "$DIR/csflags" ] && printf '  %s\n' "$("$DIR/csflags" "$PID" | head -1)"
echo
echo "--- command ---"
echo "  ./hddb break $PID $ADDR"
echo

read -n 1 -s -r -p "press any key to run it "
echo
echo

"$DIR/hddb" break "$PID" "$ADDR"
STATUS=$?

echo
echo "--- after ---"
printf '  p_flag          0x%s   P_TRACED=%s\n' "$(pflag "$PID")" "$(traced "$PID")"
[ -x "$DIR/csflags" ] && printf '  %s\n' "$("$DIR/csflags" "$PID" | head -1)"

if [ $STATUS -eq 0 ]; then
    echo
    echo "  the target trapped, we read its registers, and it is still running."
    echo "  P_TRACED never moved - detect_debugger() cannot see this."
fi

exit $STATUS
