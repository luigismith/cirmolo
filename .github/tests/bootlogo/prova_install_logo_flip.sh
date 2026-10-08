#!/bin/bash
# Banco di prova del ramo "Flip" di App/BootLogo/install_logo.sh (logo di avvio nella memoria interna).
#
# Esegue una copia dello script con dash, shell POSIX della stessa famiglia di quella di busybox sulla
# console, in una cartella temporanea: /proc/mtd, /dev/mtdNro, /dev/mtd2, /mnt/SDCARD e /tmp sono
# reindirizzati a file finti e i binari ARM (unpackbootimg, rsce_tool, mkbootimg, flashcp, ffmpeg)
# sono sostituiti da imitazioni che si possono far fallire a comando.
#
# Uso:  bash .github/tests/bootlogo/prova_install_logo_flip.sh [App/BootLogo/install_logo.sh]
set -u
RADICE="$(cd "$(dirname "$0")/../../.." && pwd)"
SRC="${1:-$RADICE/App/BootLogo/install_logo.sh}"
command -v dash > /dev/null || { echo "serve dash (in Git Bash c'e' gia')"; exit 2; }
BASE="$(mktemp -d)"
trap 'rm -rf "$BASE"' EXIT
PASS=0; FAIL=0

ok()   { echo "  OK   $1"; PASS=$((PASS+1)); }
ko()   { echo "  FAIL $1"; FAIL=$((FAIL+1)); }
check(){ if eval "$2"; then ok "$1"; else ko "$1"; fi; }

# partizioni finte: nome e dimensione (esadecimale, come in /proc/mtd)
PARTIZIONI="mtd0:00040000:uboot mtd1:00020000:misc mtd2:00100000:boot mtd3:00080000:rootfs"

prepara() {   # $1 = nome dello scenario
    T="$BASE/$1"; mkdir -p "$T/tmp" "$T/SDCARD/spruce/scripts" "$T/SDCARD/spruce/bin" "$T/SDCARD/spruce/imgs" \
        "$T/SDCARD/App/BootLogo/payload/bin" "$T/SDCARD/Saves/spruce"
    printf 'dev:    size   erasesize  name\n' > "$T/mtd"
    for p in $PARTIZIONI; do
        dev="${p%%:*}"; resto="${p#*:}"; hex="${resto%%:*}"; nome="${resto#*:}"
        printf '%s: %s 00020000 "%s"\n' "$dev" "$hex" "$nome" >> "$T/mtd"
        if [ "$dev" = mtd2 ]; then
            { printf 'ANDROID!'; head -c $((0x$hex - 8)) /dev/urandom; } > "$T/${dev}ro"
        else
            head -c $((0x$hex)) /dev/urandom > "$T/${dev}ro"
        fi
        cp "$T/${dev}ro" "$T/${dev}-iniziale.img"
    done
    cp "$T/mtd2ro" "$T/originale.img"
    : > "$T/drop_caches"
    printf 'PNG' > "$T/SDCARD/App/BootLogo/bootlogo.png"
    cat > "$T/SDCARD/spruce/scripts/helperFunctions.sh" <<EOF
export PLATFORM=Flip DISPLAY_WIDTH=640 DISPLAY_HEIGHT=480
log_message() { echo "LOG: \$*" >> "$T/log.txt"; }
display() { echo "DISPLAY: \$*" >> "$T/log.txt"; }
device_get_battery_percent() { echo "\${TEST_BATT-80}"; }
device_get_charging_status() { echo "\${TEST_CHG-Discharging}"; }
EOF
    printf '#!/bin/sh\necho "640|480|bgr24"\n' > "$T/SDCARD/spruce/bin/ffprobe"
    B="$T/SDCARD/App/BootLogo/payload/bin"
    cat > "$B/ffmpeg" <<'EOF'
#!/bin/sh
out=""; for a in "$@"; do out="$a"; done; printf 'BMPLOGO' > "$out"
EOF
    cat > "$B/unpackbootimg" <<'EOF'
#!/bin/sh
# unpackbootimg -i boot.img -o dir
[ "${TEST_UNPACK:-ok}" = "fail" ] && exit 1
printf 'KERNEL' > "$4/boot.img-kernel"; printf 'SECOND' > "$4/boot.img-second"
EOF
    cat > "$B/rsce_tool" <<'EOF'
#!/bin/sh
if [ "$1" = "-u" ]; then
    case "${TEST_RSCE:-ok}" in
        fail)  exit 1 ;;
        nodtb) printf 'OLDLOGO' > logo.bmp; printf 'OLDLOGO' > logo_kernel.bmp ;;
        *)     printf 'OLDLOGO' > logo.bmp; printf 'OLDLOGO' > logo_kernel.bmp; printf 'DTB' > rk-kernel.dtb ;;
    esac
    exit 0
fi
[ "${TEST_PACK:-ok}" = "fail" ] && exit 1
: > boot-second; while [ $# -gt 0 ]; do [ "$1" = "-p" ] && cat "$2" >> boot-second; shift; done
EOF
    cat > "$B/mkbootimg" <<'EOF'
#!/bin/sh
out=""; while [ $# -gt 0 ]; do [ "$1" = "-o" ] && out="$2"; shift; done
if [ "${TEST_MK:-ok}" = "big" ]; then { printf 'ANDROID!'; head -c 2000000 /dev/zero; } > "$out"; exit 0; fi
{ printf 'ANDROID!'; cat bootimg/boot.img-kernel bootimg/boot-second; } > "$out"
EOF
    # flashcp: TEST_FLASH=once fa fallire il primo tentativo a meta', always li fa fallire tutti
    cat > "$B/flashcp" <<EOF
#!/bin/sh
if [ "\${TEST_FLASH:-ok}" = "once" ] && [ ! -f "$T/flash_fallito" ]; then
    : > "$T/flash_fallito"; head -c 1000 "\$1" > "\$2"; exit 1
fi
[ "\${TEST_FLASH:-ok}" = "always" ] && { head -c 1000 "\$1" > "\$2"; exit 1; }
cat "\$1" > "\$2"
EOF
    chmod +x "$B"/* "$T/SDCARD/spruce/bin/ffprobe"
    # segnaposto @T@: la cartella di prova puo' stare sotto /tmp, quindi va inserita per ultima
    sed -e "s#/tmp#@T@/tmp#g" -e "s#/dev/mtd#@T@/mtd#g" -e 's#/dev/${MTD_DEV}#@T@/${MTD_DEV}#g' \
        -e "s#/proc/mtd#@T@/mtd#g" -e "s#/proc/sys/vm/drop_caches#@T@/drop_caches#g" \
        -e "s#/mnt/SDCARD#@T@/SDCARD#g" -e "s#@T@#$T#g" "$SRC" > "$T/SDCARD/App/BootLogo/install_logo.sh"
}

esegui() { (cd "$T/SDCARD/App/BootLogo" && PATH="$T/SDCARD/App/BootLogo/payload/bin:$PATH" dash ./install_logo.sh > "$T/out.txt" 2>&1); echo $?; }
BK() { echo "$T/SDCARD/Saves/spruce/bootlogo-backup/$1"; }
FULL() { echo "$T/SDCARD/Saves/spruce/bootlogo-backup/internal-flash/$1"; }
copia_completa_ok() {
    for p in $PARTIZIONI; do
        dev="${p%%:*}"; nome="${p##*:}"
        cmp -s "$(FULL "$dev-$nome.img")" "$T/$dev-iniziale.img" || return 1
    done
    [ "$(wc -l < "$(FULL md5sums.txt)")" -eq 4 ] && (cd "$(FULL '')" && md5sum -c --quiet md5sums.txt) && [ -f "$(FULL complete)" ]
}

echo "1. caso normale (batteria 80%)"
prepara normale; rc=$(esegui)
check "uscita 0" '[ "$rc" = 0 ]'
check "partizione scritta con il nuovo logo" 'grep -q BMPLOGO "$T/mtd2" && [ "$(head -c 8 "$T/mtd2")" = "ANDROID!" ]'
check "la nuova immagine contiene ancora il dtb" 'grep -q DTB "$T/mtd2"'
check "copia completa della memoria interna: 4 partizioni identiche, checksum validi" 'copia_completa_ok'
check "copia 'originale' di mtd2 identica alla partizione di partenza" 'cmp -s "$(BK mtd2-boot-original.img)" "$T/originale.img"'
check "copia 'prima dell ultimo cambio' identica" 'cmp -s "$(BK mtd2-boot-before-last-swap.img)" "$T/originale.img"'
check "messaggio finale di successo" 'grep -q "Boot logo updated successfully" "$T/log.txt"'
check "file temporanei rimossi" '[ ! -e "$T/tmp/boot.img" ] && [ ! -d "$T/tmp/bootres" ]'

echo "2. secondo cambio: copie originali intatte, copia completa non rifatta"
head -c 1048576 /dev/zero | cat "$T/mtd2" - | head -c 1048576 > "$T/mtd2ro"; cp "$T/mtd2ro" "$T/dopo_primo.img"
head -c 262144 /dev/urandom > "$T/mtd0ro"
rc=$(esegui)
check "uscita 0" '[ "$rc" = 0 ]'
check "originale di mtd2 ancora quello di partenza" 'cmp -s "$(BK mtd2-boot-original.img)" "$T/originale.img"'
check "copia recente = stato prima del secondo cambio" 'cmp -s "$(BK mtd2-boot-before-last-swap.img)" "$T/dopo_primo.img"'
check "copia completa ancora quella iniziale (mtd0 cambiato non ricopiato)" 'copia_completa_ok'

echo "3. batteria al 30% senza caricatore: si ferma prima di tutto"
prepara batteria; rc=$(TEST_BATT=30 esegui)
check "uscita 1" '[ "$rc" = 1 ]'
check "partizione non toccata, nessuna copia" '[ ! -e "$T/mtd2" ] && [ ! -e "$(FULL .)" ]'
check "messaggio batteria" 'grep -q "Battery at 30%" "$T/log.txt"'

echo "4. batteria al 30% ma in carica: procede"
prepara carica; rc=$(TEST_BATT=30 TEST_CHG=Charging esegui)
check "uscita 0" '[ "$rc" = 0 ] && grep -q BMPLOGO "$T/mtd2"'

echo "5. batteria illeggibile: si ferma"
prepara batt_vuota; rc=$(TEST_BATT="" TEST_CHG="" esegui)
check "uscita 1 e partizione non toccata" '[ "$rc" = 1 ] && [ ! -e "$T/mtd2" ]'

echo "6. lettura incompleta della partizione di avvio"
prepara lettura_corta; head -c 500000 "$T/originale.img" > "$T/mtd2ro"; rc=$(esegui)
check "uscita 1 e partizione non toccata" '[ "$rc" = 1 ] && [ ! -e "$T/mtd2" ]'
check "messaggio" 'grep -q "Couldn.t back up the internal memory\|Couldn.t read boot partition" "$T/log.txt"'

echo "7. mtd2 assente da /proc/mtd"
prepara senza_mtd2; sed -i '/mtd2:/d' "$T/mtd"; rc=$(esegui)
check "uscita 1 e partizione non toccata" '[ "$rc" = 1 ] && [ ! -e "$T/mtd2" ]'

echo "8. copia completa interrotta (mtd1 letta a meta'): non scrive e la rifara'"
prepara copia_corta; head -c 1000 "$T/mtd1-iniziale.img" > "$T/mtd1ro"; rc=$(esegui)
check "uscita 1 e partizione non toccata" '[ "$rc" = 1 ] && [ ! -e "$T/mtd2" ]'
check "nessun segno di copia completa" '[ ! -f "$(FULL complete)" ]'
check "messaggio" 'grep -q "Couldn.t back up the internal memory" "$T/log.txt"'
cp "$T/mtd1-iniziale.img" "$T/mtd1ro"; rc=$(esegui)
check "al tentativo successivo copia completa e scrittura riuscite" '[ "$rc" = 0 ] && copia_completa_ok && grep -q BMPLOGO "$T/mtd2"'

echo "9. risorse senza dtb (spacchettamento parziale): non scrive"
prepara senza_dtb; rc=$(TEST_RSCE=nodtb esegui)
check "uscita 1 e partizione non toccata" '[ "$rc" = 1 ] && [ ! -e "$T/mtd2" ]'
check "messaggio struttura inattesa" 'grep -q "Unexpected boot partition layout" "$T/log.txt"'

echo "10. rsce_tool -u fallisce: non scrive"
prepara rsce_ko; rc=$(TEST_RSCE=fail esegui)
check "uscita 1 e partizione non toccata" '[ "$rc" = 1 ] && [ ! -e "$T/mtd2" ]'

echo "11. unpackbootimg fallisce: non scrive"
prepara unpack_ko; rc=$(TEST_UNPACK=fail esegui)
check "uscita 1 e partizione non toccata" '[ "$rc" = 1 ] && [ ! -e "$T/mtd2" ]'

echo "12. nuova immagine piu' grande della partizione: non scrive"
prepara troppo_grande; rc=$(TEST_MK=big esegui)
check "uscita 1 e partizione non toccata" '[ "$rc" = 1 ] && [ ! -e "$T/mtd2" ]'
check "messaggio dimensione" 'grep -q "does not fit" "$T/log.txt"'

echo "13. scrittura interrotta: ripristino automatico della copia"
prepara flash_ko; rc=$(TEST_FLASH=once esegui)
check "uscita 1" '[ "$rc" = 1 ]'
check "partizione rimessa com'era" 'cmp -s "$T/mtd2" "$T/originale.img"'
check "messaggio ripristino" 'grep -q "Previous boot partition restored" "$T/log.txt"'

echo "14. scrittura e ripristino falliti: avviso chiaro, copia sulla SD"
prepara flash_ko2; rc=$(TEST_FLASH=always esegui)
check "uscita 1" '[ "$rc" = 1 ]'
check "avviso di non spegnere" 'grep -q "Restore failed. Keep the console on" "$T/log.txt"'
check "copia integra sulla SD" 'cmp -s "$(BK mtd2-boot-before-last-swap.img)" "$T/originale.img"'

echo
echo "Esito: $PASS superati, $FAIL falliti"
[ "$FAIL" = 0 ]
