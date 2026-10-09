#!/bin/sh
# Chiedi all'IA: microfono delle cuffie Bluetooth (sperimentale).
#   on   riavvia bluealsa con anche il profilo HFP (Hands-Free: microfono, audio a 8 kHz) e ricollega le
#        cuffie, cosi' le cuffie offrono il microfono; l'app registra con arecord da bluealsa (PROFILE=sco).
#   off  rimette bluealsa com'era (solo A2DP, l'audio di qualita' per i giochi) e ricollega le cuffie.
# Lo chiama l'app quando si sceglie il microfono nelle impostazioni; launch.sh fa "off" all'uscita.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

FLAG=/tmp/ia-bt-hfp

pidof bluetoothd >/dev/null 2>&1 || exit 0

reconnect() {
    [ -n "$1" ] || return 0
    timeout 10 bluetoothctl disconnect "$1" >/dev/null 2>&1
    sleep 2
    timeout 15 bluetoothctl connect "$1" >/dev/null 2>&1
}

mac="$(bt_connected_audio_mac)"
case "$1" in
on)
    [ -f "$FLAG" ] && exit 0
    bt_stop_bluealsa
    BT_BLUEALSA_ARGS="-p a2dp-source -p hfp-ag"
    export BT_BLUEALSA_ARGS
    bt_start_bluealsa
    unset BT_BLUEALSA_ARGS
    touch "$FLAG"
    sleep 1
    reconnect "$mac"
    log_message "Chiedi all'IA: bluealsa con HFP per il microfono Bluetooth"
    ;;
off)
    [ -f "$FLAG" ] || exit 0
    bt_stop_bluealsa
    bt_start_bluealsa
    rm -f "$FLAG"
    sleep 1
    reconnect "$mac"
    log_message "Chiedi all'IA: bluealsa di nuovo solo A2DP"
    ;;
esac
