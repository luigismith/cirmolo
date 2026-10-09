#!/bin/sh
# picoloop: synth + sequencer a passi per palmari (yoyz/picoloop, GPL, versione 0.79).
# Binario aarch64 compilato con Zig (sorgenti e patch in D:\dev\picoloop-build sul PC di Luigi):
# audio via SDL, niente MIDI; dipende solo da libSDL2 e libSDL2_ttf di PyUI.
# Dati: patch/ (suoni MDADrum), bank/ (pattern e song salvati, creata al primo avvio),
# picoloop.ini (opzionale, vedi picoloop.ini.example: velocita' di ripetizione tasti, tema).
# Tasti: A conferma/nota, B indietro, L1/R1 pagina, croce (o stick sinistro) cursore,
# SELECT+UP/DOWN volume, START play/stop. Si esce con SELECT+START oppure tenendo premuto
# MENU un secondo (menu-esci.py manda SIGTERM, che SDL trasforma in SDL_QUIT: uscita pulita).

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

APP_DIR="$(cd "$(dirname "$0")" && pwd)"

# Niente sospensione per inattivita' mentre si suona.
killall -q idlemon 2>/dev/null
killall -q idle_watchdog.sh 2>/dev/null

cd "$APP_DIR" || exit 1
export HOME="$APP_DIR"
export LD_LIBRARY_PATH="/mnt/SDCARD/App/PyUI/dll:$LD_LIBRARY_PATH"
export SDL_VIDEODRIVER=KMSDRM
export SDL_AUDIODRIVER=alsa
chmod +x "$APP_DIR/picoloop" 2>/dev/null
python3 "$APP_DIR/menu-esci.py" &
ESCI_PID=$!
./picoloop > /mnt/SDCARD/Saves/spruce/picoloop.log 2>&1
kill "$ESCI_PID" 2>/dev/null
sync
