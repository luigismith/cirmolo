#!/bin/sh
# Chiedi all'IA - funzioni IA per tutta la console (scheda Console dell'app).
#   applica  scrive nella configurazione di RetroArch il servizio IA (traduzione dei giochi con SELECT + giu')
#            o lo spegne, secondo Saves/claude/impostazioni.txt
#   avvia    applica e, se la traduzione e' attiva, fa partire il traduttore (ia-traduttore, porta 4404)
#   ferma    ferma il traduttore
# avvia e ferma li chiama spruce/scripts/emu/lib/ra_functions.sh prima e dopo RetroArch.

SETTINGS="${CIRMOLO_SAVES:-/mnt/SDCARD/Saves/claude}/impostazioni.txt"
APP_DIR="$(cd "$(dirname "$0")" && pwd)"
RA_CFG="/mnt/SDCARD/Saves/ra-configs/retroarch-${PLATFORM:-Flip}.cfg"
LOG=/mnt/SDCARD/Saves/spruce/ia-traduttore.log

get() { sed -n "s/^$1=//p" "$SETTINGS" 2>/dev/null | tail -n 1; }

set_ra() {
    grep -q "^$1 = \"$2\"\$" "$RA_CFG" && return 0     # gia' cosi': la configurazione non si riscrive
    if grep -q "^$1 = " "$RA_CFG"; then
        sed -i "s|^$1 = .*|$1 = \"$2\"|" "$RA_CFG"
    else
        echo "$1 = \"$2\"" >> "$RA_CFG"
    fi
}

# Codici delle lingue del servizio IA di RetroArch 1.22 (translation_defines.h).
ra_lang() {
    case "$1" in
        en) echo 1 ;; es) echo 2 ;; fr) echo 3 ;; it) echo 4 ;; de) echo 5 ;; ja) echo 6 ;; nl) echo 7 ;;
        cs) echo 8 ;; da) echo 9 ;; sv) echo 10 ;; hr) echo 11 ;; ko) echo 12 ;; zh-CN) echo 13 ;; zh-TW) echo 14 ;;
        ca) echo 15 ;; pl) echo 48 ;; pt) echo 49 ;; ro) echo 50 ;; ru) echo 51 ;; tr) echo 59 ;; uk) echo 60 ;;
        *) echo 4 ;;
    esac
}

# Lingua di arrivo: quella scelta, altrimenti quella dell'interfaccia di PyUI.
target_lang() {
    l="$(get traduzione.lingua)"
    if [ -z "$l" ]; then
        case "$(jq -r '.language // "Italian"' /mnt/SDCARD/App/PyUI/py-ui-config.json 2>/dev/null)" in
            English) l=en ;; Spanish) l=es ;; French) l=fr ;; German) l=de ;; Catalan) l=ca ;; Polish) l=pl ;;
            Romanian) l=ro ;; Turkish) l=tr ;; "Portuguese (BR)"|"Portuguese (Eur)") l=pt ;; *) l=it ;;
        esac
    fi
    echo "$l"
}

applica() {
    [ -f "$RA_CFG" ] || return 0
    if [ "$(get traduzione)" = "1" ]; then
        set_ra ai_service_enable true
        set_ra ai_service_mode 0                   # immagine: il riquadro con la traduzione (e la voce, se c'e')
        set_ra ai_service_pause true               # SELECT + giu' mette in pausa; di nuovo, toglie il riquadro
        set_ra ai_service_url "http://127.0.0.1:4404/"
        set_ra ai_service_source_lang 0
        set_ra ai_service_target_lang "$(ra_lang "$(target_lang)")"
        set_ra input_ai_service_btn 12             # giu' (con SELECT, il tasto delle scorciatoie)
    else
        set_ra ai_service_enable false
        set_ra input_ai_service_btn nul
    fi
}

case "$1" in
applica)
    applica
    ;;
avvia)
    applica
    [ "$(get traduzione)" = "1" ] || exit 0
    killall -q ia-traduttore 2>/dev/null
    ( cd "$APP_DIR" && exec ./ia-traduttore 4404 ) >> "$LOG" 2>&1 &
    ;;
ferma)
    killall -q ia-traduttore 2>/dev/null
    ;;
esac
exit 0
