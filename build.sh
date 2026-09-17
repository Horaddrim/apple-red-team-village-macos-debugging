#!/bin/sh

DIR="$(cd "$(dirname "$0")" && pwd)"

clang -O0 -g -Wl,-interposable -o "$DIR/target" "$DIR/target.c" || exit 1
codesign -s - -f --entitlements "$DIR/get-task-allow.plist" "$DIR/target" || exit 1

cp "$DIR/target" "$DIR/target-hardened" || exit 1
codesign -s - -f -o runtime "$DIR/target-hardened" || exit 1

clang -O0 -dynamiclib -Wl,-undefined,dynamic_lookup \
    -o "$DIR/interpose.dylib" "$DIR/interpose.c" || exit 1

# hddb needs the server half of mach_exc.defs. mig(1) generates it; the .defs
# ships in the SDK, so there is nothing vendored in this tree.
SDK="$(xcrun --show-sdk-path)"
mig -user "$DIR/mach_excUser.c" \
    -server "$DIR/mach_excServer.c" \
    -header "$DIR/mach_exc.h" \
    -I"$SDK/usr/include" \
    "$SDK/usr/include/mach/mach_exc.defs" >/dev/null || exit 1

clang -O0 -g -Wall -Wextra \
    -o "$DIR/hddb" "$DIR/hddb.c" \
    "$DIR/mach_excServer.c" "$DIR/mach_excUser.c" || exit 1

clang -O0 -g -Wall -Wextra \
    -o "$DIR/csflags" "$DIR/csflags.c" || exit 1
