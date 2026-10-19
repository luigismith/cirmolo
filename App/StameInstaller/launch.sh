#!/bin/sh
# Install Stame: downloads the latest release of Stame (github.com/luigismith/stame), a chord
# instrument for the Akai MPK mini IV and the macro pad, and installs it in App/Stame, in the Music
# area. Stame is a separate project: Cirmolo holds only this installer. The saves in Saves/stame are
# left alone.
#
# The entry stays in Music only while there is something to do: after a successful install, or when
# Stame is already up to date, it hides itself; checkUpdate.sh (run at boot) shows it again as
# "Install Stame" when App/Stame is missing, or as "Update Stame" when GitHub has a newer release.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh
. "$(cd "$(dirname "$0")" && pwd)/installerFunctions.sh"

IMG="$APP_DIR/icon.png"
HIDES="This entry hides itself until a new version comes out."

show() { display_image_and_text "$IMG" 35 20 "$1" 75; }

finish() {    # $1 = message, $2 = seconds on screen, $3 = exit code
    log_message "Stame installer: $1"
    show "$1"
    sleep "${2:-4}"
    rm -rf "$TMP"
    kill_pyui_message_writer
    exit "${3:-0}"
}

# PyUI has already quit when a script runs from the menu: a message reaches the screen only through
# the listener started here and closed in finish, as the OTA app does.
start_pyui_message_writer
show "Looking for the latest Stame..."
ping -c 2 -W 2 api.github.com > /dev/null 2>&1 || finish "No connection: turn on Wi-Fi and try again." 5 1
fetch_release
case $? in
    1) finish "Could not reach GitHub. Try again later." 5 1 ;;
    2) finish "No Stame release found on GitHub." 5 1 ;;
esac

HAVE="$(installed_version)"
if [ -n "$HAVE" ] && ! version_older_than "$HAVE" "$VER"; then
    hide_entry
    finish "Stame $HAVE is already installed: you find it in Music. $HIDES" 6
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
hide_entry
finish "Stame $VER installed: you find it in Music. $HIDES" 6
