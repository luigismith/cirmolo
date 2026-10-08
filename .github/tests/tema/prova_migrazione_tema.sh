#!/bin/bash
# Prova del passaggio una tantum al tema Cirmolo all'avvio (spruce/scripts/runtime.sh, Cirmolo 0.2):
# chi aggiorna dalla 0.1 con il tema SPRUCE passa a Cirmolo una volta sola, poi la scelta resta sua.
# Il blocco viene estratto dallo script ed eseguito con dash su una SD finta.
# Uso: bash .github/tests/tema/prova_migrazione_tema.sh
set -u
RADICE="$(cd "$(dirname "$0")/../../.." && pwd)"
T="$(mktemp -d)"
sed -n '/^# Cirmolo 0.2: il tema Cirmolo/,/^fi$/p' "$RADICE/spruce/scripts/runtime.sh" \
    | sed "s#/mnt/SDCARD#$T/SDCARD#g" > "$T/blocco.sh"
cat > "$T/prova.sh" <<EOF
log_message() { :; }
SYSTEM_JSON="$T/SDCARD/Saves/flip-system.json"
. "$T/blocco.sh"
EOF
PASS=0; FAIL=0
ok()   { echo "  OK   $1"; PASS=$((PASS+1)); }
male() { echo "  FAIL $1"; FAIL=$((FAIL+1)); }
SEGNALINO="$T/SDCARD/Saves/spruce/cirmolo-tema-0.2"

# file di sistema come lo scrive la Flip (tabulazioni, niente a capo finale)
sistema() {   # $1 riga del tema ("" = assente)
    rm -rf "$T/SDCARD"; mkdir -p "$T/SDCARD/Saves" "$T/SDCARD/Themes/Cirmolo"
    if [ -n "$1" ]; then
        printf '{\n\t"vol": 20,\n\t"wifi": 1,\n\t%s\n}' "$1" > "$T/SDCARD/Saves/flip-system.json"
    else
        printf '{\n\t"vol": 20,\n\t"wifi": 1\n}' > "$T/SDCARD/Saves/flip-system.json"
    fi
}
tema() { sed -n 's/.*"theme"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$T/SDCARD/Saves/flip-system.json"; }

sistema '"theme": "SPRUCE"'
dash "$T/prova.sh"
[ "$(tema)" = "Cirmolo" ] && ok "0.1 con SPRUCE: passa a Cirmolo" || male "0.1 con SPRUCE: tema $(tema)"
[ -f "$SEGNALINO" ] && ok "segnalino creato in Saves/spruce" || male "segnalino mancante"
grep -q '"vol": 20' "$T/SDCARD/Saves/flip-system.json" && grep -q '"wifi": 1' "$T/SDCARD/Saves/flip-system.json" \
    && ok "le altre impostazioni restano" || male "impostazioni alterate"

sed -i 's/"Cirmolo"/"SPRUCE"/' "$T/SDCARD/Saves/flip-system.json"
dash "$T/prova.sh"
[ "$(tema)" = "SPRUCE" ] && ok "SPRUCE scelto dopo il passaggio: resta" || male "scelta dell'utente sovrascritta"

sistema '"theme":"SPRUCE"'
dash "$T/prova.sh"
[ "$(tema)" = "Cirmolo" ] && ok "SPRUCE senza spazi: passa a Cirmolo" || male "SPRUCE senza spazi: tema $(tema)"

sistema '"theme": "Grape"'
dash "$T/prova.sh"
[ "$(tema)" = "Grape" ] && ok "altro tema scelto dall'utente: resta" || male "Grape cambiato in $(tema)"

sistema ''
dash "$T/prova.sh"
[ -z "$(tema)" ] && [ -f "$SEGNALINO" ] && ok "installazione nuova (tema non ancora scritto): nessuna modifica" \
    || male "installazione nuova: tema '$(tema)'"

sistema '"theme": "SPRUCE"'
rm -rf "$T/SDCARD/Themes/Cirmolo"
dash "$T/prova.sh"
[ "$(tema)" = "SPRUCE" ] && [ ! -f "$SEGNALINO" ] && ok "tema Cirmolo assente: nessuna modifica, si riprova dopo" \
    || male "tema Cirmolo assente: tema $(tema)"

rm -rf "$T"
echo "Esito: $PASS superati, $FAIL falliti"
[ "$FAIL" -eq 0 ]
