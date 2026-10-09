#!/bin/sh
# Cirmolo Sampler: campionatore a pad con sequencer a passi, registrazione dal microfono USB e MIDI.
# Sorgenti in src/; campioni registrati in /mnt/SDCARD/Saves/sampler/campioni.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

APP_DIR="$(cd "$(dirname "$0")" && pwd)"

# Niente sospensione per inattivita' mentre si suona.
killall -q idlemon 2>/dev/null
killall -q idle_watchdog.sh 2>/dev/null

cd "$APP_DIR" || exit 1
"$APP_DIR/cirmolo-sampler" > "/mnt/SDCARD/Saves/spruce/cirmolo-sampler.log" 2>&1
sync
