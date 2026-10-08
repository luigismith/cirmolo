#!/bin/sh
# Chiedi a Claude: chat con Claude tramite l'API di Anthropic (servono il Wi-Fi e una chiave API nel file
# Saves/claude/chiave.txt). Le richieste HTTPS le fa il curl di spruce. Sorgenti in src/.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

APP_DIR="$(cd "$(dirname "$0")" && pwd)"

# Niente sospensione per inattivita' mentre si scrive o si legge una risposta lunga.
killall -q idlemon 2>/dev/null
killall -q idle_watchdog.sh 2>/dev/null

cd "$APP_DIR" || exit 1
"$APP_DIR/claude-chat" > "/mnt/SDCARD/Saves/spruce/claude-chat.log" 2>&1
sync
