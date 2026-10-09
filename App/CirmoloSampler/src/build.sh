#!/bin/sh
# Compila Cirmolo Sampler con Zig (cc di clang + glibc 2.27 per la Flip).
#   ZIG=/percorso/zig SDL_INCLUDE=/percorso/SDL2/include sh build.sh [prova]
# Senza argomenti produce ../cirmolo-sampler (aarch64, per la console);
# con "prova" anche sampler-test.exe e cirmolo-sampler.exe per Windows in $OUT (predefinito: ./build).
# Grafica, font, MIDI e piattaforma vengono dal kit comune: spruce/cirmolo-kit.
set -e
cd "$(dirname "$0")"
ZIG="${ZIG:-zig}"
SDL_INCLUDE="${SDL_INCLUDE:?serve SDL_INCLUDE con gli header di SDL 2.32}"
OUT="${OUT:-build}"
KIT=../../../spruce/cirmolo-kit
CFLAGS="-O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function -I. -I$KIT"
SRC="app.c sampler.c launchpad.c $KIT/gfx.c $KIT/midi.c"

"$ZIG" cc $CFLAGS -s -target aarch64-linux-gnu.2.27 -mcpu=cortex_a55 -I"$SDL_INCLUDE" \
    -o ../cirmolo-sampler sampler_main.c $KIT/platform.c $SRC -lm -ldl
echo "console: ../cirmolo-sampler"

if [ "$1" = "prova" ]; then
    mkdir -p "$OUT"
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -o "$OUT/sampler-test.exe" main_test.c $SRC
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -I"$SDL_INCLUDE" -o "$OUT/cirmolo-sampler.exe" sampler_main.c $KIT/platform.c $SRC
    echo "PC: $OUT/sampler-test.exe (prove senza finestra), $OUT/cirmolo-sampler.exe (serve SDL2.dll)"
fi
