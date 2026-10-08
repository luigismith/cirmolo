#!/bin/bash
# Prova del confronto di versioni del controllo aggiornamenti all'avvio (spruce/scripts/runtimeHelper.sh):
# con CIRMOLO_VERSION nel feed si confrontano le versioni di Cirmolo, senza si resta sul confronto di spruce.
# Il blocco di confronto viene estratto dallo script ed eseguito con dash su feed finti.
# Uso: bash .github/tests/ota/prova_confronto_versioni.sh
set -u
RADICE="$(cd "$(dirname "$0")/../../.." && pwd)"
T="$(mktemp -d)"
sed -n '/# Extract version info from downloaded file/,/if \[ \$update_available -eq 1 \]; then/p' \
    "$RADICE/spruce/scripts/runtimeHelper.sh" | sed '$d' | sed "s#/mnt/SDCARD/spruce/cirmolo#$T/cirmolo#g" > "$T/blocco.sh"
cat > "$T/prova.sh" <<EOF
flag_check() { return 1; }
get_config_value() { echo "\$2"; }
log_message() { :; }
TMP_DIR="$T"
CURRENT_VERSION="\$1"
. "$T/blocco.sh"
echo "\$update_available"
EOF
PASS=0; FAIL=0
prova() {   # $1 descrizione, $2 feed, $3 versione di spruce installata, $4 versione Cirmolo ("" = assente), $5 atteso
    printf '%b' "$2" > "$T/spruce"
    if [ -n "$4" ]; then printf '%s\n' "$4" > "$T/cirmolo"; else rm -f "$T/cirmolo"; fi
    got="$(dash "$T/prova.sh" "$3")"
    if [ "$got" = "$5" ]; then echo "  OK   $1"; PASS=$((PASS+1)); else echo "  FAIL $1: $got invece di $5"; FAIL=$((FAIL+1)); fi
}
prova "Cirmolo 0.1.0, feed 0.2.0: aggiornamento"      'RELEASE_VERSION=4.5.2\nCIRMOLO_VERSION=0.2.0\n' 4.5.2 0.1.0 1
prova "Cirmolo 0.1.0, feed 0.1.0: niente"             'RELEASE_VERSION=4.5.2\nCIRMOLO_VERSION=0.1.0\n' 4.5.2 0.1.0 0
prova "Cirmolo 0.2.0, feed 0.1.0 (vecchio): niente"   'RELEASE_VERSION=4.5.2\nCIRMOLO_VERSION=0.1.0\n' 4.5.2 0.2.0 0
prova "Cirmolo 0.9.3, feed 0.10.0: aggiornamento"     'RELEASE_VERSION=4.5.2\nCIRMOLO_VERSION=0.10.0\n' 4.5.2 0.9.3 1
prova "spruce puro, feed di spruce 4.5.3: aggiornamento" 'RELEASE_VERSION=4.5.3\n' 4.5.2 "" 1
prova "spruce puro, feed di spruce 4.5.2: niente"     'RELEASE_VERSION=4.5.2\n' 4.5.2 "" 0
prova "feed Cirmolo su spruce puro: confronto spruce" 'RELEASE_VERSION=4.5.2\nCIRMOLO_VERSION=0.2.0\n' 4.5.2 "" 0
rm -rf "$T"
echo "Esito: $PASS superati, $FAIL falliti"
[ "$FAIL" = 0 ]
