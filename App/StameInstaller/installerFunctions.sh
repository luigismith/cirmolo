#!/bin/sh
# Shared by launch.sh (the install, from the menu) and checkUpdate.sh (the silent check at boot).
# Stame (github.com/luigismith/stame) is a separate project: Cirmolo holds only this installer.

APP_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG="$APP_DIR/config.json"
REPO=luigismith/stame
DEST=/mnt/SDCARD/App/Stame
TMP=/mnt/SDCARD/Saves/stame-install
LOGF=/mnt/SDCARD/Saves/spruce/stame-install.log
GET_TIMEOUT=900
DESC_INSTALL="Download and install (or update) Stame, the chord instrument for the MPK mini IV and the macro pad. Wi-Fi needed."
DESC_UPDATE="A new version of Stame is available: download and install it. Wi-Fi needed."

get() {       # $1 = URL, $2 = file; without certificate checks only if TLS fails, as the OTA does
    curl -sSfL -m "$GET_TIMEOUT" -o "$2" "$1" 2>>"$LOGF" && return 0
    curl -ksSfL -m "$GET_TIMEOUT" -o "$2" "$1" 2>>"$LOGF"
}

# The latest release on GitHub: sets VER (without the v), URL (the .7z) and SUM (its .sha256).
# Returns 1 when GitHub cannot be reached and 2 when there is no release.
fetch_release() {
    mkdir -p "$TMP" "$(dirname "$LOGF")"
    get "https://api.github.com/repos/$REPO/releases/latest" "$TMP/release.json" || return 1
    VER="$(jq -r '.tag_name // empty' "$TMP/release.json" | sed 's/^[vV]//')"
    URL="$(jq -r '.assets[] | select(.name | endswith(".7z")) | .browser_download_url' "$TMP/release.json" | head -n 1)"
    SUM="$(jq -r '.assets[] | select(.name | endswith(".7z.sha256")) | .browser_download_url' "$TMP/release.json" | head -n 1)"
    [ -n "$VER" ] && [ -n "$URL" ] || return 2
}

# The installed version; empty when Stame is missing, or when it has no VERSION file (a package
# unpacked by hand): then the installer installs the latest release over it.
installed_version() {
    [ -f "$DEST/stame" ] && cat "$DEST/VERSION" 2>/dev/null
}

# The entry in the Music area shows itself only while there is something to do. As spruce does with
# -FirmwareUpdate-, hiding it means renaming "label" to "#label" in config.json: PyUI skips an app
# without a label, and it re-reads the App folder every time the menu opens.
show_entry() {    # $1 = label, $2 = description
    jq --arg l "$1" --arg d "$2" 'del(."#label") | .label = $l | .description = $d' "$CONFIG" > "$CONFIG.tmp" &&
        mv "$CONFIG.tmp" "$CONFIG"
}

hide_entry() {
    jq --arg d "$DESC_INSTALL" 'if .label then ."#label" = .label | del(.label) else . end | .description = $d' \
        "$CONFIG" > "$CONFIG.tmp" && mv "$CONFIG.tmp" "$CONFIG"
}
