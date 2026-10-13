#!/bin/sh
# Cirmolo Synth: groovebox con 26 batterie sintetizzate, basso acido, synth West Coast e sequencer
# (sorgenti in src/). Stato e registrazioni in /mnt/SDCARD/Saves/cirmolo-synth.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

APP_DIR="$(cd "$(dirname "$0")" && pwd)"

# Niente sospensione per inattivita' mentre si suona (come fa il file manager).
killall -q idlemon 2>/dev/null
killall -q idle_watchdog.sh 2>/dev/null

cd "$APP_DIR" || exit 1
"$APP_DIR/cirmolo-synth" > "/mnt/SDCARD/Saves/spruce/cirmolo-synth.log" 2>&1
sync
