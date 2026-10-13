#!/bin/sh
# Compila Cirmolo Synth (groovebox) con Zig (cc di clang + glibc 2.27 per la Flip).
#   ZIG=/percorso/zig SDL_INCLUDE=/percorso/SDL2/include sh build.sh [prova]
# Senza argomenti produce ../cirmolo-synth (aarch64, per la console);
# con "prova" anche, in $OUT (predefinito: ./build), per Windows: gb-test.exe (prove del motore),
# gb-ui-test.exe (prove dell'interfaccia, con le schermate) e cirmolo-synth.exe (serve SDL2.dll).
# Grafica, font e piattaforma vengono dal kit comune: spruce/cirmolo-kit.
set -e
cd "$(dirname "$0")"
ZIG="${ZIG:-zig}"
SDL_INCLUDE="${SDL_INCLUDE:?serve SDL_INCLUDE con gli header di SDL 2.32}"
OUT="${OUT:-build}"
KIT=../../../spruce/cirmolo-kit
CFLAGS="-O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function -I. -I$KIT"
DSP="gb_dsp.c gb_kits.c"
SRC="gb_app.c $DSP $KIT/gfx.c $KIT/midi.c"

"$ZIG" cc $CFLAGS -s -target aarch64-linux-gnu.2.27 -mcpu=cortex_a55 -I"$SDL_INCLUDE" \
    -o ../cirmolo-synth gb_main.c $KIT/platform.c $SRC -lm -ldl
echo "console: ../cirmolo-synth"

if [ "$1" = "prova" ]; then
    mkdir -p "$OUT"
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -o "$OUT/gb-test.exe" gb_test.c $DSP
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -o "$OUT/gb-ui-test.exe" gb_ui_test.c $SRC
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -I"$SDL_INCLUDE" -o "$OUT/cirmolo-synth.exe" gb_main.c $KIT/platform.c $SRC
    echo "PC: $OUT/gb-test.exe, $OUT/gb-ui-test.exe <font> <uscita>, $OUT/cirmolo-synth.exe (serve SDL2.dll)"
fi
