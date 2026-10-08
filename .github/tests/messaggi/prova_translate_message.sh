#!/bin/bash
# Prova di translate_message() (spruce/scripts/helperFunctions.sh) con dash, senza la console:
# la funzione viene estratta da helperFunctions.sh e /mnt/SDCARD reindirizzato a una cartella di prova.
# Serve jq 1.7: variabile JQ, oppure jq nel PATH. Su Windows la riga di comando altera le barre
# rovesciate, quindi qui si provano solo testi senza "\" (quelli li copre test_messaggi_script.py).
#
# Uso:  JQ=/percorso/jq bash .github/tests/messaggi/prova_translate_message.sh
set -u
RADICE="$(cd "$(dirname "$0")/../../.." && pwd)"
JQ="${JQ:-$(command -v jq)}"
[ -n "$JQ" ] || { echo "serve jq"; exit 2; }
command -v dash > /dev/null || { echo "serve dash"; exit 2; }
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
mkdir -p "$T/SDCARD/App/PyUI/lang" "$T/SDCARD/spruce/scripts" "$T/bin"
cp "$RADICE/App/PyUI/lang/Italian.json" "$RADICE/App/PyUI/lang/English.json" "$T/SDCARD/App/PyUI/lang/"
cp "$RADICE/spruce/scripts/translate_message.jq" "$T/SDCARD/spruce/scripts/"
printf '#!/bin/sh\nexec "%s" "$@" | tr -d "\\r"\n' "$JQ" > "$T/bin/jq"; chmod +x "$T/bin/jq"
# solo la funzione, con i percorsi della prova
sed -n '/^translate_message() {/,/^}/p' "$RADICE/spruce/scripts/helperFunctions.sh" | sed "s#/mnt/SDCARD#$T/SDCARD#g" > "$T/tm.sh"
PASS=0; FAIL=0
prova() {   # $1 descrizione, $2 lingua (vuota = nessuna configurazione), $3 testo, $4 atteso
    if [ -n "$2" ]; then printf '{"language": "%s"}' "$2" > "$T/SDCARD/App/PyUI/py-ui-config.json"; else rm -f "$T/SDCARD/App/PyUI/py-ui-config.json"; fi
    out="$(PATH="$T/bin:$PATH" dash -c ". '$T/tm.sh'; translate_message \"\$1\"" x "$3")"
    if [ "$out" = "$4" ]; then echo "  OK   $1"; PASS=$((PASS+1)); else echo "  FAIL $1: «$out» invece di «$4»"; FAIL=$((FAIL+1)); fi
}
prova "italiano, voce esatta"            Italian "Checking for updates..."  "Ricerca aggiornamenti in corso..."
prova "italiano, con segnaposto"         Italian "Downloading update 2 of 5..." "Download dell'aggiornamento 2 di 5 in corso..."
prova "italiano, logo di avvio"          Italian "Battery at 30%: charge above 50% or plug in the charger. Cancelling boot logo swap." "Batteria al 30%: caricala oltre il 50% o collega il caricatore. Cambio del logo annullato."
prova "italiano, a capo vero nel testo"  Italian "Syncthing Check:
No folders configured" "Controllo Syncthing:
Nessuna cartella configurata"
prova "italiano, testo sconosciuto"      Italian "Something else" "Something else"
prova "inglese: invariato"               English "Checking for updates..." "Checking for updates..."
prova "senza configurazione: invariato"  ""      "Checking for updates..." "Checking for updates..."
prova "lingua senza file: invariato"     Klingon "Checking for updates..." "Checking for updates..."
prova "testo vuoto"                      Italian "" ""
printf 'non e json' > "$T/SDCARD/App/PyUI/lang/Rotto.json"
prova "file di lingua rotto: invariato"  Rotto   "Checking for updates..." "Checking for updates..."
echo "Esito: $PASS superati, $FAIL falliti"
[ "$FAIL" = 0 ]
