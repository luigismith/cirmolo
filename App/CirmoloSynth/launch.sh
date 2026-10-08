#!/bin/sh
# Cirmolo Synth: sintetizzatore, batteria e sequencer (sorgenti in src/).

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

APP_DIR="$(cd "$(dirname "$0")" && pwd)"

# Niente sospensione per inattivita' mentre si suona (come fa il file manager).
killall -q idlemon 2>/dev/null
killall -q idle_watchdog.sh 2>/dev/null

cd "$APP_DIR" || exit 1
"$APP_DIR/cirmolo-synth" > "/mnt/SDCARD/Saves/spruce/cirmolo-synth.log" 2>&1
sync
