#!/bin/sh
# OpenOrc: sintetizzatore di accordi alla maniera dell'Orchid, per la Akai MPK mini IV collegata alla
# porta USB-C (funziona anche con i tasti della Flip). Sorgenti in src/.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

APP_DIR="$(cd "$(dirname "$0")" && pwd)"

# Niente sospensione per inattivita' mentre si suona.
killall -q idlemon 2>/dev/null
killall -q idle_watchdog.sh 2>/dev/null

cd "$APP_DIR" || exit 1
"$APP_DIR/openorc" > "/mnt/SDCARD/Saves/spruce/openorc.log" 2>&1
sync
