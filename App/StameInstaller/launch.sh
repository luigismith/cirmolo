#!/bin/sh
# Install Stame: downloads the latest release of Stame (github.com/luigismith/stame), a chord
# instrument for the Akai MPK mini IV and the macro pad, and installs it in App/Stame, in the Music
# area. Stame is a separate project: Cirmolo holds only this installer. Run it again to update; the
# saves in Saves/stame are left alone.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

APP_DIR="$(cd "$(dirname "$0")" && pwd)"
IMG="$APP_DIR/icon.png"
REPO=luigismith/stame
DEST=/mnt/SDCARD/App/Stame
TMP=/mnt/SDCARD/Saves/stame-install
LOGF=/mnt/SDCARD/Saves/spruce/stame-install.log

show() { display_image_and_text "$IMG" 35 20 "$1" 75; }

finish() {    # $1 = message, $2 = seconds on screen, $3 = exit code
    log_message "Stame installer: $1"
    show "$1"
    sleep "${2:-4}"
    rm -rf "$TMP"
    display_kill
    exit "${3:-0}"
}

get() {       # $1 = URL, $2 = file; without certificate checks only if TLS fails, as the OTA does
    curl -sSfL -m 900 -o "$2" "$1" 2>>"$LOGF" && return 0
    curl -ksSfL -m 900 -o "$2" "$1" 2>>"$LOGF"
}

mkdir -p "$TMP" "$(dirname "$LOGF")"
show "Looking for the latest Stame..."
ping -c 2 -W 2 api.github.com > /dev/null 2>&1 || finish "No connection: turn on Wi-Fi and try again." 5 1
get "https://api.github.com/repos/$REPO/releases/latest" "$TMP/release.json" ||
    finish "Could not reach GitHub. Try again later." 5 1
VER="$(jq -r '.tag_name // empty' "$TMP/release.json" | sed 's/^[vV]//')"
URL="$(jq -r '.assets[] | select(.name | endswith(".7z")) | .browser_download_url' "$TMP/release.json" | head -n 1)"
SUM="$(jq -r '.assets[] | select(.name | endswith(".7z.sha256")) | .browser_download_url' "$TMP/release.json" | head -n 1)"
[ -n "$VER" ] && [ -n "$URL" ] || finish "No Stame release found on GitHub." 5 1

HAVE="$(cat "$DEST/VERSION" 2>/dev/null)"
if [ -n "$HAVE" ] && [ -f "$DEST/stame" ] && ! version_older_than "$HAVE" "$VER"; then
    finish "Stame $HAVE is already installed: you find it in Music." 4
fi

show "Downloading Stame $VER (about 70 MB)..."
get "$URL" "$TMP/Stame.7z" || finish "Download failed. Try again." 5 1
if [ -n "$SUM" ] && command -v sha256sum > /dev/null 2>&1 && get "$SUM" "$TMP/Stame.7z.sha256"; then
    [ "$(cut -c1-64 "$TMP/Stame.7z.sha256")" = "$(sha256sum "$TMP/Stame.7z" | cut -d' ' -f1)" ] ||
        finish "The download is damaged. Try again." 5 1
fi

show "Installing Stame $VER..."
7zr x -y -o"$TMP/x" "$TMP/Stame.7z" >> "$LOGF" 2>&1 || finish "Could not unpack Stame. Is the SD card full?" 5 1
[ -f "$TMP/x/App/Stame/stame" ] || finish "The package is not complete." 5 1
rm -rf "$DEST.old"
[ -d "$DEST" ] && mv "$DEST" "$DEST.old"
if mv "$TMP/x/App/Stame" "$DEST"; then
    rm -rf "$DEST.old"
else
    [ -d "$DEST.old" ] && mv "$DEST.old" "$DEST"
    finish "Could not install Stame." 5 1
fi
chmod +x "$DEST/launch.sh" "$DEST/stame" 2>/dev/null
sync
finish "Stame $VER installed: you find it in Music." 5
