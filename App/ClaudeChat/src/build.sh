#!/bin/sh
# Compila Chiedi all'IA con Zig (cc di clang + glibc 2.27 per la Flip).
#   ZIG=/percorso/zig SDL_INCLUDE=/percorso/SDL2/include sh build.sh [prova]
# Senza argomenti produce ../claude-chat (aarch64, per la console); con "prova" anche chat-test.exe
# (prove senza finestra e senza rete) e claude-chat.exe per Windows in $OUT. Grafica e piattaforma
# vengono dal kit comune (spruce/cirmolo-kit), fornitori, rete e voce da spruce/cirmolo-ia. Le richieste HTTPS le fa curl (quello di spruce).
set -e
cd "$(dirname "$0")"
ZIG="${ZIG:-zig}"
SDL_INCLUDE="${SDL_INCLUDE:?serve SDL_INCLUDE con gli header di SDL 2.32}"
OUT="${OUT:-build}"
KIT=../../../spruce/cirmolo-kit
IA=../../../spruce/cirmolo-ia
CFLAGS="-O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function -I. -I$KIT -I$IA"
SRC="chat_app.c $IA/conv.c $IA/claude.c $IA/openai.c $IA/providers.c $IA/voice.c $IA/net.c $IA/json.c $KIT/gfx.c $KIT/i18n.c"

"$ZIG" cc $CFLAGS -s -target aarch64-linux-gnu.2.27 -mcpu=cortex_a55 -I"$SDL_INCLUDE" \
    -o ../claude-chat chat_main.c $KIT/platform.c $KIT/midi.c $SRC -lm -ldl
echo "console: ../claude-chat"
TRAD="traduttore.c $IA/ask.c $IA/png.c $IA/conv.c $IA/claude.c $IA/openai.c $IA/providers.c $IA/voice.c $IA/net.c $IA/json.c $KIT/gfx.c $KIT/i18n.c"
"$ZIG" cc $CFLAGS -s -target aarch64-linux-gnu.2.27 -mcpu=cortex_a55 -o ../ia-traduttore $TRAD -lm
echo "console: ../ia-traduttore (traduttore dei giochi per RetroArch)"

if [ "$1" = "prova" ]; then
    mkdir -p "$OUT"
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -o "$OUT/chat-test.exe" main_test.c $SRC $IA/ask.c $IA/png.c
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -I"$SDL_INCLUDE" -o "$OUT/claude-chat.exe" chat_main.c $KIT/platform.c $KIT/midi.c $SRC
    "$ZIG" cc $CFLAGS -target x86_64-windows-gnu -o "$OUT/ia-traduttore.exe" $TRAD
    echo "PC: $OUT/chat-test.exe (prove), $OUT/claude-chat.exe (serve SDL2.dll; sul PC non si collega)"
fi
