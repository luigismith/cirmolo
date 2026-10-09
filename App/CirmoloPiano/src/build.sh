#!/bin/sh
# Compila Cirmolo Piano con Zig (cc di clang + glibc 2.27 per la Flip).
#   ZIG=/percorso/zig SDL_INCLUDE=/percorso/SDL2/include sh build.sh [prova]
# Senza argomenti produce ../cirmolo-piano (aarch64, per la console);
# con "prova" anche piano-test.exe e cirmolo-piano.exe per Windows in $OUT (predefinito: ./build).
# Grafica, font, MIDI e piattaforma vengono dal kit comune (spruce/cirmolo-kit), il sintetizzatore
# SoundFont da spruce/cirmolo-kit/third_party/tsf.h.
set -e
cd "$(dirname "$0")"
ZIG="${ZIG:-zig}"
SDL_INCLUDE="${SDL_INCLUDE:?serve SDL_INCLUDE con gli header di SDL 2.32}"
OUT="${OUT:-build}"
KIT=../../../spruce/cirmolo-kit
CFLAGS="-O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function -I. -I$KIT"
SRC="app.c piano.c $KIT/gfx.c $KIT/midi.c"

"$ZIG" cc $CFLAGS -s -target aarch64-linux-gnu.2.27 -mcpu=cortex_a55 -I"$SDL_INCLUDE" \
    -o ../cirmolo-piano piano_main.c $KIT/platform.c $SRC -lm -ldl
echo "console: ../cirmolo-piano"

if [ "$1" = "prova" ]; then
    mkdir -p "$OUT"
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -o "$OUT/piano-test.exe" main_test.c $SRC
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -I"$SDL_INCLUDE" -o "$OUT/cirmolo-piano.exe" piano_main.c $KIT/platform.c $SRC
    echo "PC: $OUT/piano-test.exe (prove senza finestra), $OUT/cirmolo-piano.exe (serve SDL2.dll e CIRMOLO_SF2)"
fi
