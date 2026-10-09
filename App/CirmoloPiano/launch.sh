#!/bin/sh
# Cirmolo Piano: strumento General MIDI (SoundFont) per una tastiera MIDI USB o i tasti della Flip.
# Sorgenti in src/, SoundFont in sf2/.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

APP_DIR="$(cd "$(dirname "$0")" && pwd)"

# Niente sospensione per inattivita' mentre si suona.
killall -q idlemon 2>/dev/null
killall -q idle_watchdog.sh 2>/dev/null

cd "$APP_DIR" || exit 1
"$APP_DIR/cirmolo-piano" > "/mnt/SDCARD/Saves/spruce/cirmolo-piano.log" 2>&1
sync
