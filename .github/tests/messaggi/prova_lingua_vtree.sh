#!/bin/bash
# Prova del blocco di App/FileManagement/launch.sh che allinea la lingua di vTree a quella di PyUI.
# Serve jq 1.7 (variabile JQ o nel PATH) e dash.
# Uso:  JQ=/percorso/jq bash .github/tests/messaggi/prova_lingua_vtree.sh
set -u
RADICE="$(cd "$(dirname "$0")/../../.." && pwd)"
FM="$RADICE/App/FileManagement"
JQ="${JQ:-$(command -v jq)}"
[ -n "$JQ" ] || { echo "serve jq"; exit 2; }
T="$(mktemp -d)"
mkdir -p "$T/h/lang" "$T/bin"
printf '#!/bin/sh\nexec "%s" "$@" | tr -d "\\r"\n' "$JQ" > "$T/bin/jq"
chmod +x "$T/bin/jq"
cp "$FM/lang/Italian.ini" "$FM/lang/English.ini" "$T/h/lang/"
sed -n '/# Cirmolo: same language/,/sed -i "s\/^Language/p' "$FM/launch.sh" | sed "s#/mnt/SDCARD/App/PyUI#$T#g" > "$T/blocco.sh"
PASS=0; FAIL=0
prova() {   # $1 lingua di PyUI, $2 lingua iniziale di vTree, $3 attesa
    printf '{"language":"%s"}' "$1" > "$T/py-ui-config.json"
    printf '[General]\nShowHidden=true\nLanguage=%s\n' "$2" > "$T/h/config.ini"
    HOME="$T/h" PATH="$T/bin:$PATH" dash "$T/blocco.sh"
    got="$(sed -n 's/^Language=//p' "$T/h/config.ini")"
    if [ "$got" = "$3" ] && grep -q '^ShowHidden=true$' "$T/h/config.ini"; then
        echo "  OK   PyUI $1, vTree $2 -> $got"; PASS=$((PASS+1))
    else
        echo "  FAIL PyUI $1, vTree $2 -> $got (atteso $3)"; FAIL=$((FAIL+1))
    fi
}
prova Italian English Italian
prova English Italian English
prova Klingon Italian English
prova 'Ba/d' Italian English
prova '' Italian English
echo "Esito: $PASS superati, $FAIL falliti"
[ "$FAIL" = 0 ]
