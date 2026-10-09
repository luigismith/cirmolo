#!/bin/sh
# Chiedi all'IA - azioni sulla console per gli strumenti del modello (comandi a voce o scritti).
#   stato            batteria, carica, ora, Wi-Fi, spazio libero, temperatura, volume e luminosita'
#   luminosita N     luminosita' dello schermo 0-10 (come PyUI: 10 = 255 sul pannello)
#   volume N         volume 0-20 (con set_volume di spruce, che sceglie altoparlante o cuffie)
# Stampa una riga di risposta per il modello.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

tz="$(jq -r '.timezone // empty' /mnt/SDCARD/Saves/spruce/shared-system.json 2>/dev/null)"
[ -n "$tz" ] && [ -f "/mnt/SDCARD/spruce/zoneinfo/$tz" ] && export TZ=":/mnt/SDCARD/spruce/zoneinfo/$tz"

level_to_panel() {
    case "$1" in
        10) echo 255 ;; 9) echo 225 ;; 8) echo 200 ;; 7) echo 175 ;; 6) echo 150 ;;
        5) echo 125 ;; 4) echo 100 ;; 3) echo 75 ;; 2) echo 50 ;; 1) echo 25 ;; *) echo 1 ;;
    esac
}

num() { case "$1" in ''|*[!0-9]*) echo -1 ;; *) echo "$1" ;; esac; }

case "$1" in
stato)
    bat="$(cat /sys/class/power_supply/battery/capacity 2>/dev/null)"
    st="$(cat /sys/class/power_supply/battery/status 2>/dev/null)"
    ac="$(cat /sys/class/power_supply/ac/online 2>/dev/null)"
    temp="$(cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null)"
    ssid="$(wpa_cli -i wlan0 status 2>/dev/null | sed -n 's/^ssid=//p')"
    ip="$(wpa_cli -i wlan0 status 2>/dev/null | sed -n 's/^ip_address=//p')"
    free="$(df -h /mnt/SDCARD 2>/dev/null | awk 'NR==2 {print $4 " free of " $2}')"
    vol="$(jq -r '.vol // empty' "$SYSTEM_JSON" 2>/dev/null)"
    bl="$(jq -r '.backlight // empty' "$SYSTEM_JSON" 2>/dev/null)"
    echo "battery ${bat:-?}% (${st:-?}, charger $( [ "$ac" = 1 ] && echo connected || echo not connected )); time $(date '+%A %d %B %Y %H:%M'); Wi-Fi ${ssid:-not connected}${ip:+ ($ip)}; SD card ${free:-?}; CPU temperature $(( ${temp:-0} / 1000 )) C; volume ${vol:-?}/20; screen brightness ${bl:-?}/10"
    ;;
luminosita)
    n="$(num "$2")"
    if [ "$n" -lt 0 ] || [ "$n" -gt 10 ]; then echo "error: brightness level must be 0-10"; exit 0; fi
    level_to_panel "$n" > /sys/class/backlight/backlight/brightness
    jq ".backlight = $n" "$SYSTEM_JSON" > "$SYSTEM_JSON.tmp" && mv "$SYSTEM_JSON.tmp" "$SYSTEM_JSON"
    echo "screen brightness set to $n/10"
    ;;
volume)
    n="$(num "$2")"
    if [ "$n" -lt 0 ] || [ "$n" -gt 20 ]; then echo "error: volume level must be 0-20"; exit 0; fi
    set_volume "$n" > /dev/null 2>&1
    echo "volume set to $n/20"
    ;;
*)
    echo "error: unknown action"
    ;;
esac
