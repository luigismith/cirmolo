#!/bin/sh
# Diapason: accordatore (serve un microfono USB-C), note di riferimento e metronomo. Sorgenti in src/.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

APP_DIR="$(cd "$(dirname "$0")" && pwd)"

# Niente sospensione per inattivita' mentre il metronomo suona.
killall -q idlemon 2>/dev/null
killall -q idle_watchdog.sh 2>/dev/null

cd "$APP_DIR" || exit 1
"$APP_DIR/diapason" > "/mnt/SDCARD/Saves/spruce/diapason.log" 2>&1
sync
