#!/bin/sh
# Compila Chiedi all'IA e i suoi programmi per la console con Zig (cc di clang + glibc 2.27 per la Flip).
#   ZIG=/percorso/zig SDL_INCLUDE=/percorso/SDL2/include sh build.sh [prova]
# Produce in .. (aarch64): claude-chat (la chat), ia-traduttore (traduttore dei giochi per RetroArch),
# ia-scheda (scheda del gioco e promemoria "dove eri rimasto"), ia-diario (diario delle partite).
# Con "prova" anche, in $OUT, i programmi per Windows: chat-test.exe e scheda-test.exe (prove senza
# finestra e senza rete), claude-chat.exe, ia-traduttore.exe e ia-diario.exe (con --prova o le variabili
# CIRMOLO_*). Grafica e piattaforma vengono dal kit comune (spruce/cirmolo-kit), fornitori, rete, voce e
# diario da spruce/cirmolo-ia. Le richieste HTTPS le fa curl (quello di spruce).
set -e
cd "$(dirname "$0")"
ZIG="${ZIG:-zig}"
SDL_INCLUDE="${SDL_INCLUDE:?serve SDL_INCLUDE con gli header di SDL 2.32}"
OUT="${OUT:-build}"
KIT=../../../spruce/cirmolo-kit
IA=../../../spruce/cirmolo-ia
CFLAGS="-O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function -I. -I$KIT -I$IA"
AI="$IA/conv.c $IA/claude.c $IA/openai.c $IA/providers.c $IA/voice.c $IA/net.c $IA/json.c $KIT/i18n.c"
ARM="-s -target aarch64-linux-gnu.2.27 -mcpu=cortex_a55"
SRC="chat_app.c $IA/mdtext.c $AI $KIT/gfx.c"
TRAD="traduttore.c $IA/iaconf.c $IA/ask.c $IA/png.c $AI $KIT/gfx.c"
SCHEDA="scheda_app.c $IA/iaconf.c $IA/ask.c $IA/mdtext.c $IA/diario.c $AI $KIT/gfx.c"
DIARIO="diario_main.c $IA/diario.c $IA/iaconf.c $IA/ask.c $AI"
CONSIGLI="consigli_app.c $IA/collezione.c $IA/diario.c $IA/iaconf.c $IA/ask.c $IA/mdtext.c $AI $KIT/gfx.c"

"$ZIG" cc $CFLAGS $ARM -I"$SDL_INCLUDE" -o ../claude-chat chat_main.c $KIT/platform.c $KIT/midi.c $SRC -lm -ldl
echo "console: ../claude-chat"
"$ZIG" cc $CFLAGS $ARM -o ../ia-traduttore $TRAD -lm
echo "console: ../ia-traduttore (traduttore dei giochi per RetroArch)"
"$ZIG" cc $CFLAGS $ARM -I"$SDL_INCLUDE" -o ../ia-scheda scheda_main.c $KIT/platform.c $KIT/midi.c $SCHEDA -lm -ldl
echo "console: ../ia-scheda (scheda del gioco e promemoria)"
"$ZIG" cc $CFLAGS $ARM -o ../ia-diario $DIARIO -lm
echo "console: ../ia-diario (diario delle partite)"
"$ZIG" cc $CFLAGS $ARM -I"$SDL_INCLUDE" -o ../ia-consigli consigli_main.c $KIT/platform.c $KIT/midi.c $CONSIGLI -lm -ldl
echo "console: ../ia-consigli (Cosa gioco?, si apre da App/CosaGioco)"

if [ "$1" = "prova" ]; then
    mkdir -p "$OUT"
    W="-target x86_64-windows-gnu"
    "$ZIG" cc $CFLAGS $W -o "$OUT/chat-test.exe" main_test.c $SRC $IA/ask.c $IA/png.c
    "$ZIG" cc $CFLAGS $W -I"$SDL_INCLUDE" -o "$OUT/claude-chat.exe" chat_main.c $KIT/platform.c $KIT/midi.c $SRC
    "$ZIG" cc $CFLAGS $W -o "$OUT/ia-traduttore.exe" $TRAD
    "$ZIG" cc $CFLAGS $W -o "$OUT/scheda-test.exe" scheda_test.c $SCHEDA
    "$ZIG" cc $CFLAGS $W -o "$OUT/ia-diario.exe" $DIARIO
    "$ZIG" cc $CFLAGS $W -o "$OUT/consigli-test.exe" consigli_test.c $CONSIGLI
    echo "PC: chat-test.exe e scheda-test.exe (prove), claude-chat.exe (serve SDL2.dll), ia-traduttore.exe, ia-diario.exe in $OUT"
fi
