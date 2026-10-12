#!/bin/sh
# Fioritura: giardino generativo di suoni e luci (fiori che tornano, lucciole che rimbalzano), con
# tastiera MIDI o Launchpad Mini MK3 sulla porta USB-C. Sorgenti in src/; stato e registrazioni in
# /mnt/SDCARD/Saves/fioritura.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

APP_DIR="$(cd "$(dirname "$0")" && pwd)"

# Niente sospensione per inattivita' mentre si suona (il sogno continua da solo).
killall -q idlemon 2>/dev/null
killall -q idle_watchdog.sh 2>/dev/null

cd "$APP_DIR" || exit 1
"$APP_DIR/fioritura" > "/mnt/SDCARD/Saves/spruce/fioritura.log" 2>&1
sync
