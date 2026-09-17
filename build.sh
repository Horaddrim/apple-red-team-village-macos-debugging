#!/bin/sh

DIR="$(cd "$(dirname "$0")" && pwd)"

clang -O0 -g -Wl,-interposable -o "$DIR/target" "$DIR/target.c" || exit 1
codesign -s - -f --entitlements "$DIR/get-task-allow.plist" "$DIR/target" || exit 1

cp "$DIR/target" "$DIR/target-hardened" || exit 1
codesign -s - -f -o runtime "$DIR/target-hardened" || exit 1

clang -O0 -dynamiclib -Wl,-undefined,dynamic_lookup \
    -o "$DIR/interpose.dylib" "$DIR/interpose.c" || exit 1

clang -O0 -g -Wall -Wextra \
    -o "$DIR/mach_vm_adventures" "$DIR/mach_vm_adventures.c" || exit 1

clang -O0 -g -Wall -Wextra \
    -o "$DIR/csflags" "$DIR/csflags.c" || exit 1
