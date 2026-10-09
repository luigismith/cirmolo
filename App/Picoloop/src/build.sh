#!/bin/sh
# Compila picoloop (yoyz/picoloop, GPL) per la Miyoo Flip con Zig come compilatore cross.
#   sh build.sh            compila (solo i file cambiati) e linka ./picoloop
#   sh build.sh pulisci    cancella gli oggetti e ricompila tutto
# Variabili: ZIG (zig.exe), SRC (sorgenti di picoloop), JOBS (compilazioni in parallelo).
#
# Scelte:
# - target aarch64, glibc 2.27, Cortex-A55 (RK3566); libc++ di Zig linkata statica,
#   quindi il binario non dipende da libstdc++.so.6 della console;
# - audio via SDL (AudioDriverSDL.cpp, -D__SDL_AUDIO__): niente RtAudio/ALSA diretto;
# - niente RtMidi (-DMIYOO_FLIP toglie __RTMIDI__ in Master.h): MidiInSystem/MidiOutSystem
#   e il motore MIDIOUT restano fuori;
# - font e logo sono embedded (EmbeddedAssets.cpp, PC_DESKTOP) ma il rendering del testo
#   usa SDL_ttf: si linka libSDL2_ttf-2.0.so di PyUI (freetype/harfbuzz statici dentro).
set -e
cd "$(dirname "$0")"
HERE="$(pwd)"
ZIG="${ZIG:-/d/dev/toolchain/zig-x86_64-windows-0.17.0/zig.exe}"
SRC="${SRC:-/d/dev/picoloop/picoloop}"
JOBS="${JOBS:-8}"
DLL="${DLL:-/d/dev/cirmolo/App/PyUI/dll}"
OBJ="$HERE/obj"
OUT="$HERE/picoloop"

TARGET="-target aarch64-linux-gnu.2.27 -mcpu=cortex_a55"

SYNTH="-DENABLE_SYNTH_PICOSYNTH -DENABLE_SYNTH_PICODRUM -DENABLE_SYNTH_OPL2 -DENABLE_SYNTH_PBSYNTH"
SYNTH="$SYNTH -DENABLE_SYNTH_LGPTSAMPLER -DENABLE_SYNTH_CURSYNTH -DENABLE_SYNTH_OPEN303"
SYNTH="$SYNTH -DENABLE_SYNTH_TWYTCHSYNTH -DENABLE_SYNTH_MDADRUM -DENABLE_SYNTH_SIDSYNTH"

# -w: i sorgenti producono migliaia di avvisi con clang; restano visibili solo gli errori.
# -Wno-c++11-narrowing: Twytch (twytchhelm_helm_oscillators.cpp) inizializza float con
#   espressioni double in una lista {}: per clang e' un errore, per g++ un avviso.
CXXFLAGS="-std=c++11 -O2 -fpermissive -w -Wno-c++11-narrowing -include cstdio -include cstdlib -include cstring -include string -include cmath"
CXXFLAGS="$CXXFLAGS -I$SRC -I$HERE/include"
CXXFLAGS="$CXXFLAGS -D__LINUX__ -DLINUX -DPC_DESKTOP -DMIYOO_FLIP -DSCREEN_MULT=2 -D__SDL20__ -D__SDL_AUDIO__"
CXXFLAGS="$CXXFLAGS -DDEBUGPRINTF -DDEBUGPRINTF_STDOUT $SYNTH"

if [ "$1" = "pulisci" ]; then
    rm -rf "$OBJ"
fi
mkdir -p "$OBJ"

# Elenco dei sorgenti: tutti i .cpp di Makefile_sources (righe commentate escluse) tranne il
# motore MidiOutSystem, piu' i file di piattaforma del Makefile rg35xxsp senza RtAudio/RtMidi.
SOURCES="AudioDriverSDL.cpp SYSTEMLINUX.cpp PicoloopIni.cpp EmbeddedAssets.cpp"
SOURCES="$SOURCES $(grep -v '^[[:space:]]*#' "$SRC/Makefile_sources" | grep -o '[A-Za-z0-9_./-]*\.cpp' | grep -v '^Machine/MidiOutSystem/' | tr '\n' ' ')"

rm -f "$OBJ"/*.err
OBJECTS=""
n=0
for s in $SOURCES; do
    o="$OBJ/$(echo "$s" | sed 's#/#__#g; s#\.cpp$#.o#')"
    OBJECTS="$OBJECTS $o"
    if [ -f "$o" ] && [ ! "$SRC/$s" -nt "$o" ] && [ ! "$HERE/build.sh" -nt "$o" ]; then
        continue
    fi
    echo "  c++ $s"
    # In background: l'errore viene segnato con un file .err (wait senza argomenti torna sempre 0).
    ( "$ZIG" c++ $TARGET $CXXFLAGS -c "$SRC/$s" -o "$o" 2>"$o.log" || { cat "$o.log"; rm -f "$o"; touch "$o.err"; } ) &
    n=$((n+1))
    if [ "$n" -ge "$JOBS" ]; then
        wait
        n=0
    fi
done
wait
if ls "$OBJ"/*.err >/dev/null 2>&1; then
    echo "ERRORI di compilazione in: $(ls "$OBJ"/*.err | sed 's#.*/##; s#\.o\.err##' | tr '\n' ' ')"
    exit 1
fi

# Gli header .h non sono tracciati: se cambia un header, usare "pulisci".
# Si linka direttamente contro le .so di PyUI (DT_NEEDED = SONAME libSDL2-2.0.so.0 e
# libSDL2_ttf-2.0.so.0); -s toglie i simboli (da 28 MB a pochi MB).
echo "  link $OUT"
"$ZIG" c++ $TARGET -s -o "$OUT" $OBJECTS "$DLL/libSDL2-2.0.so" "$DLL/libSDL2_ttf-2.0.so" -lpthread -lm -ldl
python -I "$HERE/elfdeps.py" "$OUT"
echo "fatto: $OUT"
