#!/bin/sh
# Compila OpenOrc con Zig (cc di clang + glibc 2.27 per la Flip).
#   ZIG=/percorso/zig SDL_INCLUDE=/percorso/SDL2/include sh build.sh [prova]
# Senza argomenti produce ../openorc (aarch64, per la console); con "prova" anche orc-test.exe
# (prove senza finestra) e openorc.exe per Windows in $OUT. Grafica, MIDI e piattaforma vengono dal
# kit comune: spruce/cirmolo-kit.
set -e
cd "$(dirname "$0")"
ZIG="${ZIG:-zig}"
SDL_INCLUDE="${SDL_INCLUDE:?serve SDL_INCLUDE con gli header di SDL 2.32}"
OUT="${OUT:-build}"
KIT=../../../spruce/cirmolo-kit
IA=../../../spruce/cirmolo-ia
CFLAGS="-O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function -I. -I$KIT -I$IA"
# Configurazione delle tastiere con l'IA: fornitori, chiavi e rete di Chiedi all'IA (spruce/cirmolo-ia).
AI="orc_ia.c $IA/ask.c $IA/iaconf.c $IA/conv.c $IA/claude.c $IA/openai.c $IA/providers.c $IA/net.c $IA/json.c $KIT/i18n.c"
SRC="orc_app.c orc_dsp.c chords.c $AI $KIT/gfx.c $KIT/midi.c"

"$ZIG" cc $CFLAGS -s -target aarch64-linux-gnu.2.27 -mcpu=cortex_a55 -I"$SDL_INCLUDE" \
    -o ../openorc orc_main.c $KIT/platform.c $SRC -lm -ldl
echo "console: ../openorc"

if [ "$1" = "prova" ]; then
    mkdir -p "$OUT"
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -o "$OUT/orc-test.exe" main_test.c $SRC
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -I"$SDL_INCLUDE" -o "$OUT/openorc.exe" orc_main.c $KIT/platform.c $SRC
    echo "PC: $OUT/orc-test.exe (prove senza finestra), $OUT/openorc.exe (serve SDL2.dll)"
fi
