#!/bin/bash
# Prova dei passaggi una tantum al tema di serie all'avvio (spruce/scripts/runtime.sh):
# - Cirmolo 0.2: chi arriva dalla 0.1 con il tema SPRUCE passa a Cirmolo;
# - Cirmolo 0.4: chi ha Cirmolo passa a STAME; chi ha scelto un altro tema, SPRUCE compreso, lo tiene.
# Ogni passaggio avviene una volta sola, poi la scelta resta all'utente.
# I due blocchi vengono estratti dallo script ed eseguiti con dash su una SD finta.
# Uso: bash .github/tests/tema/prova_migrazione_tema.sh
set -u
RADICE="$(cd "$(dirname "$0")/../../.." && pwd)"
T="$(mktemp -d)"
{ sed -n '/^# Cirmolo 0.2: il tema Cirmolo/,/^fi$/p' "$RADICE/spruce/scripts/runtime.sh"
  sed -n '/^# Cirmolo 0.4: il tema STAME/,/^fi$/p' "$RADICE/spruce/scripts/runtime.sh"
} | sed "s#/mnt/SDCARD#$T/SDCARD#g" > "$T/blocco.sh"
cat > "$T/prova.sh" <<EOF
log_message() { :; }
SYSTEM_JSON="$T/SDCARD/Saves/flip-system.json"
. "$T/blocco.sh"
EOF
PASS=0; FAIL=0
ok()   { echo "  OK   $1"; PASS=$((PASS+1)); }
male() { echo "  FAIL $1"; FAIL=$((FAIL+1)); }
SEGNALINO_02="$T/SDCARD/Saves/spruce/cirmolo-tema-0.2"
SEGNALINO_04="$T/SDCARD/Saves/spruce/cirmolo-tema-stame-0.4"

grep -q '^# Cirmolo 0.4: il tema STAME' "$T/blocco.sh" && ok "blocchi 0.2 e 0.4 estratti" || male "blocco 0.4 non trovato"

# file di sistema come lo scrive la Flip (tabulazioni, niente a capo finale); $2 = versione di partenza
sistema() {   # $1 riga del tema ("" = assente)
    rm -rf "$T/SDCARD"; mkdir -p "$T/SDCARD/Saves/spruce" "$T/SDCARD/Themes/Cirmolo" "$T/SDCARD/Themes/STAME"
    if [ -n "$1" ]; then
        printf '{\n\t"vol": 20,\n\t"wifi": 1,\n\t%s\n}' "$1" > "$T/SDCARD/Saves/flip-system.json"
    else
        printf '{\n\t"vol": 20,\n\t"wifi": 1\n}' > "$T/SDCARD/Saves/flip-system.json"
    fi
    [ "${2:-0.1}" = "0.3" ] && touch "$SEGNALINO_02"
    return 0
}
tema() { sed -n 's/.*"theme"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$T/SDCARD/Saves/flip-system.json"; }

sistema '"theme": "SPRUCE"'
dash "$T/prova.sh"
[ "$(tema)" = "STAME" ] && ok "0.1 con SPRUCE: passa a STAME" || male "0.1 con SPRUCE: tema $(tema)"
[ -f "$SEGNALINO_02" ] && [ -f "$SEGNALINO_04" ] && ok "segnalini 0.2 e 0.4 creati in Saves/spruce" || male "segnalini mancanti"
grep -q '"vol": 20' "$T/SDCARD/Saves/flip-system.json" && grep -q '"wifi": 1' "$T/SDCARD/Saves/flip-system.json" \
    && ok "le altre impostazioni restano" || male "impostazioni alterate"

sed -i 's/"STAME"/"Cirmolo"/' "$T/SDCARD/Saves/flip-system.json"
dash "$T/prova.sh"
[ "$(tema)" = "Cirmolo" ] && ok "Cirmolo scelto dopo il passaggio: resta" || male "scelta dell'utente sovrascritta"

sistema '"theme": "Cirmolo"' 0.3
dash "$T/prova.sh"
[ "$(tema)" = "STAME" ] && ok "0.3 con Cirmolo: passa a STAME" || male "0.3 con Cirmolo: tema $(tema)"

sistema '"theme":"Cirmolo"' 0.3
dash "$T/prova.sh"
[ "$(tema)" = "STAME" ] && ok "Cirmolo senza spazi: passa a STAME" || male "Cirmolo senza spazi: tema $(tema)"

sistema '"theme": "SPRUCE"' 0.3
dash "$T/prova.sh"
[ "$(tema)" = "SPRUCE" ] && [ -f "$SEGNALINO_04" ] && ok "0.3 con SPRUCE scelto dall'utente: resta" \
    || male "SPRUCE scelto cambiato in $(tema)"

sistema '"theme": "Grape"' 0.3
dash "$T/prova.sh"
[ "$(tema)" = "Grape" ] && ok "altro tema scelto dall'utente: resta" || male "Grape cambiato in $(tema)"

sistema ''
dash "$T/prova.sh"
[ -z "$(tema)" ] && [ -f "$SEGNALINO_02" ] && [ -f "$SEGNALINO_04" ] \
    && ok "installazione nuova (tema non ancora scritto): nessuna modifica" || male "installazione nuova: tema '$(tema)'"

sistema '"theme": "Cirmolo"' 0.3
rm -rf "$T/SDCARD/Themes/STAME"
dash "$T/prova.sh"
[ "$(tema)" = "Cirmolo" ] && [ ! -f "$SEGNALINO_04" ] && ok "tema STAME assente: nessuna modifica, si riprova dopo" \
    || male "tema STAME assente: tema $(tema)"

sistema '"theme": "SPRUCE"'
rm -rf "$T/SDCARD/Themes/Cirmolo"
dash "$T/prova.sh"
[ "$(tema)" = "SPRUCE" ] && [ ! -f "$SEGNALINO_02" ] && ok "0.1 con SPRUCE e tema Cirmolo assente: nessuna modifica" \
    || male "tema Cirmolo assente: tema $(tema)"

rm -rf "$T"
echo "Esito: $PASS superati, $FAIL falliti"
[ "$FAIL" -eq 0 ]
