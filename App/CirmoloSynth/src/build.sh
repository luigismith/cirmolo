#!/bin/sh
# Compila Cirmolo Synth con Zig (cc di clang + glibc 2.27 per la Flip).
#   ZIG=/percorso/zig SDL_INCLUDE=/percorso/SDL2/include sh build.sh [prova]
# Senza argomenti produce ../cirmolo-synth (aarch64, per la console);
# con "prova" anche synth-test.exe e cirmolo-synth.exe per Windows in $OUT (predefinito: ./build).
set -e
cd "$(dirname "$0")"
ZIG="${ZIG:-zig}"
SDL_INCLUDE="${SDL_INCLUDE:?serve SDL_INCLUDE con gli header di SDL 2.32}"
OUT="${OUT:-build}"
CFLAGS="-O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function"
SRC="app.c gfx.c synth.c"

"$ZIG" cc $CFLAGS -s -target aarch64-linux-gnu.2.27 -mcpu=cortex_a55 -I"$SDL_INCLUDE" \
    -o ../cirmolo-synth main_sdl.c $SRC -lm -ldl
echo "console: ../cirmolo-synth"

if [ "$1" = "prova" ]; then
    mkdir -p "$OUT"
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -o "$OUT/synth-test.exe" main_test.c $SRC
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -I"$SDL_INCLUDE" -o "$OUT/cirmolo-synth.exe" main_sdl.c $SRC
    echo "PC: $OUT/synth-test.exe (prove senza finestra), $OUT/cirmolo-synth.exe (serve SDL2.dll)"
fi
