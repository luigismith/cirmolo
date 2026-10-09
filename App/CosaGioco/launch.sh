#!/bin/sh
# Cosa gioco?: tre domande e il modello di Chiedi all'IA sceglie 4 giochi della SD (il programma e le
# traduzioni stanno in App/ClaudeChat). Il gioco scelto parte da qui, dopo l'uscita dall'app: principal.sh
# cancella /tmp/cmd_to_run.sh quando l'app si chiude, quindi l'app scrive /tmp/ia-gioca.sh.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

IA_DIR=/mnt/SDCARD/App/ClaudeChat
GIOCA=/tmp/ia-gioca.sh

rm -f "$GIOCA"
cd "$IA_DIR" || exit 1
"$IA_DIR/ia-consigli" > "/mnt/SDCARD/Saves/spruce/ia-consigli.log" 2>&1
sync

if [ -s "$GIOCA" ] && sh -n "$GIOCA" 2>/dev/null; then
    # come principal.sh per i giochi scelti in PyUI: registro delle attivita' e ripresa automatica
    cmd="$(sed 's/[[:space:]]*$//' "$GIOCA")"
    cp "$GIOCA" "$FLAGS_DIR/lastgame.lock"
    set_performance
    log_activity_event "$cmd" "START"
    sh "$GIOCA" > /dev/null 2>&1
    log_activity_event "$cmd" "STOP"
    rm -f "$GIOCA"
    sync
fi
