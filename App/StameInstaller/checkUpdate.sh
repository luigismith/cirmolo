#!/bin/sh
# The silent check, run in the background at boot by update_checker (spruce/scripts/runtimeHelper.sh)
# for every App/*/checkUpdate.sh: shows the entry as "Install Stame" when App/Stame is missing, as
# "Update Stame" when GitHub has a newer release than the installed one, and hides it otherwise.
# Without a network it leaves the entry as it is. Nothing on screen, one line in the log.

. /mnt/SDCARD/spruce/scripts/helperFunctions.sh
. "$(cd "$(dirname "$0")" && pwd)/installerFunctions.sh"
TMP=/tmp/stame-check
GET_TIMEOUT=60

HAVE="$(installed_version)"
if [ -z "$HAVE" ]; then
    show_entry "Install Stame" "$DESC_INSTALL"
    log_message "Stame check: not installed; Install Stame is in Music"
    exit 0
fi
network_is_connected || exit 0
if ! fetch_release; then
    log_message "Stame check: no release info from GitHub"
    rm -rf "$TMP"
    exit 0
fi
rm -rf "$TMP"
if version_older_than "$HAVE" "$VER"; then
    show_entry "Update Stame" "$DESC_UPDATE"
    log_message "Stame check: $VER is out and $HAVE is installed; Update Stame is in Music"
else
    hide_entry
    log_message "Stame check: $HAVE is up to date; the entry is hidden"
fi
