#!/bin/bash

DIR="$(cd "$(dirname "$0")" && pwd)"
PASS=amindneedsbookslikeaswordneedsawhetstone

G=$'\033[32m'
D=$'\033[2m'
R=$'\033[31m'
BOLD=$'\033[1m'
N=$'\033[0m'

hdr()  { printf '\n%s%s>> %s%s\n\n' "$BOLD" "$G" "$*" "$N"; }
cmd()  { printf '%s$ %s%s\n' "$D" "$*" "$N"; }
warn() { printf '%s%s%s\n' "$R" "$*" "$N"; }

tpid() { pgrep -n -x target; }
hpid() { pgrep -n -x target-hardened; }

step() {
    local k
    printf '%s(demo) %s' "$D" "$N"
    while true; do
        IFS= read -rsn1 k
        case "$k" in
            s|S|'') printf 's\n\n'; return 0 ;;
            q|Q)    printf 'q\n';   exit 0 ;;
            m|M)    printf 'm\n\n'; return 1 ;;
        esac
    done
}

preflight() {
    local missing=""
    local f
    for f in target target-hardened interpose.dylib hddb csflags breakpoint_demo.sh; do
        [ -e "$DIR/$f" ] || missing="$missing $f"
    done
    if [ -n "$missing" ]; then
        warn "faltando:$missing"
        printf '  rode %s./build.sh%s uma vez e volte aqui\n' "$BOLD" "$N"
        exit 1
    fi
}

start_targets() {
    [ -n "$(tpid)" ] || ( "$DIR/target" "$PASS" hold >/dev/null 2>&1 & )
    [ -n "$(hpid)" ] || ( "$DIR/target-hardened" "$PASS" hold >/dev/null 2>&1 & )
    sleep 1
}

need_targets() {
    if [ -z "$(tpid)" ] || [ -z "$(hpid)" ]; then
        warn "targets parados, subindo..."
        start_targets
    fi
}

d1() {
    hdr "DYLD_INTERPOSE - hook que nem toca no kernel"
    cmd "./target wrong"
    step || return
    "$DIR/target" wrong
    step || return
    cmd "DYLD_INSERT_LIBRARIES=./interpose.dylib ./target wrong"
    step || return
    DYLD_INSERT_LIBRARIES="$DIR/interpose.dylib" "$DIR/target" wrong
}

d2() {
    hdr "o que da pra ver no binario, parado"
    cmd "./native_target_functions.sh"
    step || return
    "$DIR/native_target_functions.sh"
    step || return
    cmd "./disarm_target_functions.sh detect_debugger"
    step || return
    "$DIR/disarm_target_functions.sh" detect_debugger | head -18
}

d3() {
    hdr "detect_debugger - o anti-debug tripando ao vivo"
    cmd "./target $PASS"
    step || return
    "$DIR/target" "$PASS"
    step || return
    cmd "./lldb_demo.sh          (no lldb: continue, depois quit)"
    step || return
    "$DIR/lldb_demo.sh"
}

d4() {
    hdr "lendo a senha da memoria do processo vivo"
    need_targets
    cmd "./reveal_password.sh"
    step || return
    "$DIR/reveal_password.sh"
}

d5() {
    hdr "mesmo comando, alvo hardened"
    need_targets
    cmd "./reveal_password.sh \"\" target-hardened"
    step || return
    "$DIR/reveal_password.sh" "" target-hardened
}

d6() {
    hdr "entitlements lado a lado"
    cmd "codesign -d --entitlements - /usr/bin/vmmap"
    step || return
    codesign -d --entitlements - /usr/bin/vmmap 2>/dev/null
    step || return
    cmd "codesign -d --entitlements - ./hddb"
    step || return
    codesign -d --entitlements - "$DIR/hddb" 2>&1
}

d7() {
    hdr "single-step na trap -45"
    need_targets
    cmd "./lldb_trap_demo.sh \"\" target     (si, si, finish, register read x0)"
    step || return
    "$DIR/lldb_trap_demo.sh" "" target
    step || return
    cmd "./lldb_trap_demo.sh                (o mesmo, no alvo hardened)"
    step || return
    "$DIR/lldb_trap_demo.sh"
}

d8() {
    hdr "no fim das contas, e um bit"
    need_targets
    cmd "./csflags \$(pgrep -n -x target)"
    step || return
    "$DIR/csflags" "$(tpid)"
    step || return
    cmd "./csflags \$(pgrep -n -x target-hardened)"
    step || return
    "$DIR/csflags" "$(hpid)"
    step || return
    cmd "./csflags 1"
    step || return
    "$DIR/csflags" 1
}

d9() {
    hdr "nosso proprio debugger - brk #0 sem ptrace"
    need_targets
    cmd "./breakpoint_demo.sh"
    step || return
    "$DIR/breakpoint_demo.sh"
    step || return
    cmd "./csflags \$(pgrep -n -x target)     (CS_DEBUGGED nao aparece)"
    step || return
    "$DIR/csflags" "$(tpid)"
}

FN=(d1 d2 d3 d4 d5 d6 d7 d8 d9)
LB=(interpose binario detect_debugger reveal hardened entitlements trap csflags hddb)
DS=("DYLD_INTERPOSE, sem kernel"
    "nm / otool / disarm"
    "anti-debug no lldb"
    "senha da memoria do process"
    "o mesmo read, bloqueado"
    "vmmap vs a nossa tool"
    "si na trap, x0 = 0 vs 5"
    "o bit que decide tudo"
    "breakpoint nosso, P_TRACED=0")
CUR=0

menu() {
    printf '\033[H\033[2J'
    printf '%s%s  debugging macOS  ·  demos%s\n\n' "$BOLD" "$G" "$N"
    local i mark
    for i in "${!FN[@]}"; do
        if [ "$i" -eq "$CUR" ]; then mark="${G}>${N}"; else mark=" "; fi
        printf '  %s %d  %-16s %s%s%s\n' "$mark" "$((i + 1))" "${LB[$i]}" "$D" "${DS[$i]}" "$N"
    done
    printf '\n  %ss%s proximo   %s1-9%s pular   %st%s targets   %sb%s build   %sq%s sair\n' \
        "$G" "$N" "$G" "$N" "$G" "$N" "$G" "$N" "$G" "$N"
    printf '  %s(dentro de um demo: s avanca, m volta pro menu, q sai)%s\n\n' "$D" "$N"
    if [ -n "$(tpid)" ] && [ -n "$(hpid)" ]; then
        printf '  %starget=%s   target-hardened=%s%s\n' "$D" "$(tpid)" "$(hpid)" "$N"
    else
        warn "  targets parados - aperte t"
    fi
}

run_current() {
    "${FN[$CUR]}"
    CUR=$(( (CUR + 1) % ${#FN[@]} ))
    printf '\n%s-- fim. qualquer tecla pro menu --%s' "$D" "$N"
    IFS= read -rsn1
}

preflight

while true; do
    menu
    printf '\n%s(demo) %s' "$D" "$N"
    IFS= read -rsn1 k
    case "$k" in
        s|S|'') printf 's\n'; run_current ;;
        [1-9])  printf '%s\n' "$k"; CUR=$(( k - 1 )); run_current ;;
        t|T)    printf 't\n'
                pkill -x target 2>/dev/null
                pkill -x target-hardened 2>/dev/null
                sleep 1
                start_targets ;;
        b|B)    printf 'b\n'
                "$DIR/build.sh" && printf '%sbuild ok%s\n' "$G" "$N"
                sleep 1 ;;
        q|Q)    printf 'q\n'; exit 0 ;;
    esac
done
