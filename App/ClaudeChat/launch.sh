#!/bin/sh
# Chiedi all'IA: chat con i modelli di intelligenza artificiale (Claude per primo, poi OpenAI, Gemini,
# DeepSeek, Qwen, GLM, Kimi, MiniMax, Groq, OpenRouter, Mistral, Cerebras e server propri), anche a voce.
# Servono il Wi-Fi e una chiave API in Saves/claude/chiavi/<fornitore>.txt. Le richieste HTTPS le fa il
# curl di spruce. Sorgenti in src/.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

APP_DIR="$(cd "$(dirname "$0")" && pwd)"
SETTINGS=/mnt/SDCARD/Saves/claude/impostazioni.txt

# Niente sospensione per inattivita' mentre si scrive, si parla o si ascolta una risposta lunga.
killall -q idlemon 2>/dev/null
killall -q idle_watchdog.sh 2>/dev/null

cd "$APP_DIR" || exit 1
# Microfono Bluetooth scelto l'ultima volta: bluealsa con HFP (in sottofondo, l'app parte subito).
if grep -q '^mic=1' "$SETTINGS" 2>/dev/null; then
    sh "$APP_DIR/bt-microfono.sh" on &
fi
rm -f /tmp/ia-gioca.sh
"$APP_DIR/claude-chat" > "/mnt/SDCARD/Saves/spruce/claude-chat.log" 2>&1
sh "$APP_DIR/bt-microfono.sh" off
sync

# "apri Zelda": lo strumento avvia_gioco scrive il comando in /tmp/ia-gioca.sh e l'app si chiude;
# il gioco parte da qui (principal.sh cancella /tmp/cmd_to_run.sh quando l'app esce), come da PyUI.
if [ -s /tmp/ia-gioca.sh ] && sh -n /tmp/ia-gioca.sh 2>/dev/null; then
    cmd="$(sed 's/[[:space:]]*$//' /tmp/ia-gioca.sh)"
    cp /tmp/ia-gioca.sh "$FLAGS_DIR/lastgame.lock"
    set_performance
    log_activity_event "$cmd" "START"
    sh /tmp/ia-gioca.sh > /dev/null 2>&1
    log_activity_event "$cmd" "STOP"
    rm -f /tmp/ia-gioca.sh
    sync
fi
