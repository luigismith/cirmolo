#!/bin/sh
# Compila Diapason con Zig (cc di clang + glibc 2.27 per la Flip).
#   ZIG=/percorso/zig SDL_INCLUDE=/percorso/SDL2/include sh build.sh [prova]
# Senza argomenti produce ../diapason (aarch64, per la console); con "prova" anche diapason-test.exe
# e diapason.exe per Windows in $OUT. Grafica e piattaforma vengono dal kit comune: spruce/cirmolo-kit.
set -e
cd "$(dirname "$0")"
ZIG="${ZIG:-zig}"
SDL_INCLUDE="${SDL_INCLUDE:?serve SDL_INCLUDE con gli header di SDL 2.32}"
OUT="${OUT:-build}"
KIT=../../../spruce/cirmolo-kit
CFLAGS="-O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function -I. -I$KIT"

"$ZIG" cc $CFLAGS -s -target aarch64-linux-gnu.2.27 -mcpu=cortex_a55 -I"$SDL_INCLUDE" \
    -o ../diapason diapason.c $KIT/platform.c $KIT/gfx.c $KIT/midi.c -lm -ldl
echo "console: ../diapason"

if [ "$1" = "prova" ]; then
    mkdir -p "$OUT"
    "$ZIG" cc $CFLAGS -DDIAPASON_TEST -target x86_64-windows-gnu -o "$OUT/diapason-test.exe" main_test.c diapason.c $KIT/gfx.c
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -I"$SDL_INCLUDE" -o "$OUT/diapason.exe" diapason.c $KIT/platform.c $KIT/gfx.c $KIT/midi.c
    echo "PC: $OUT/diapason-test.exe, $OUT/diapason.exe (serve SDL2.dll)"
fi
