#!/bin/sh
# LittleGPTracker («piggy»): tracker a campioni nato per i palmari con gamepad, stile LSDj.
# Binario aarch64 della release ufficiale 1.5.0 (djdiskmachine/LittleGPTracker, GPLv3, build RG35XXPLUS):
# dipende solo da SDL2, qui quella di PyUI. I progetti (cartelle lgpt_*) stanno in questa cartella.
# Si esce da «Exit» nella schermata dei progetti, oppure tenendo premuto MENU (menu-esci.py).

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
python3 "$APP_DIR/menu-esci.py" &
ESCI_PID=$!
./lgpt > /mnt/SDCARD/Saves/spruce/lgpt.log 2>&1
kill "$ESCI_PID" 2>/dev/null
sync
