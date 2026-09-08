#!/usr/bin/env bash
#
# gta-orange-proton.sh - run GTA:Orange with the Steam (Proton) version of
# Grand Theft Auto V on Linux.
#
# What it does:
#   1. finds your Steam installation, the library that holds GTA V and the
#      Proton prefix of the game (steamapps/compatdata/271590)
#   2. starts GTA V through Steam (unless it is already running)
#   3. waits for GTA5.exe to appear and then runs
#         Launcher.exe --inject
#      inside the very same Proton prefix, which injects orange-core.dll into
#      the running game
#
# Usage:
#   ./gta-orange-proton.sh [options] [-- extra Launcher.exe options]
#
# Options:
#   --client-dir DIR   folder with Launcher.exe + orange-core.dll
#                      (default: the folder of this script)
#   --proton DIR       Proton installation to use (default: the one Steam
#                      configured for GTA V, read from compatdata/config_info)
#   --no-launch        do not start the game, only wait for GTA5.exe and inject
#   --timeout SEC      how long to wait for GTA5.exe (default 600)
#   -h, --help         show this help
#
# Environment overrides: ORANGE_CLIENT_DIR, STEAM_ROOT, PROTON_DIR, GTA_APPID
#
# Alternative without this script (needs protontricks):
#   protontricks-launch --appid 271590 /path/to/Launcher.exe --inject

set -euo pipefail

APPID="${GTA_APPID:-271590}"
CLIENT_DIR="${ORANGE_CLIENT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
PROTON_DIR="${PROTON_DIR:-}"
STEAM_ROOT="${STEAM_ROOT:-}"
TIMEOUT=600
LAUNCH=1
EXTRA_ARGS=()

usage() { sed -n '2,32p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }
log() { printf '[gta-orange] %s\n' "$*" >&2; }
die() { log "ERROR: $*"; exit 1; }

while [ $# -gt 0 ]; do
	case "$1" in
		--client-dir) CLIENT_DIR="$2"; shift 2 ;;
		--proton) PROTON_DIR="$2"; shift 2 ;;
		--no-launch) LAUNCH=0; shift ;;
		--timeout) TIMEOUT="$2"; shift 2 ;;
		-h|--help) usage; exit 0 ;;
		--) shift; EXTRA_ARGS+=("$@"); break ;;
		*) EXTRA_ARGS+=("$1"); shift ;;
	esac
done

[ -f "$CLIENT_DIR/Launcher.exe" ] || die "Launcher.exe not found in '$CLIENT_DIR' (use --client-dir)"
[ -f "$CLIENT_DIR/orange-core.dll" ] || die "orange-core.dll not found in '$CLIENT_DIR'"

# --- Steam root ---------------------------------------------------------------
find_steam_root() {
	local candidate
	for candidate in "$STEAM_ROOT" \
		"$HOME/.steam/steam" \
		"$HOME/.local/share/Steam" \
		"$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam" \
		"$HOME/snap/steam/common/.local/share/Steam"; do
		if [ -n "$candidate" ] && [ -d "$candidate/steamapps" ]; then
			echo "$candidate"
			return 0
		fi
	done
	return 1
}
STEAM_ROOT="$(find_steam_root)" || die "Steam installation not found (set STEAM_ROOT)"
log "Steam: $STEAM_ROOT"

# --- library folder + Proton prefix of the game -------------------------------
find_compatdata() {
	local lib
	local libs=("$STEAM_ROOT")
	if [ -f "$STEAM_ROOT/steamapps/libraryfolders.vdf" ]; then
		while IFS= read -r lib; do
			libs+=("$lib")
		done < <(grep -oE '"path"[[:space:]]+"[^"]+"' "$STEAM_ROOT/steamapps/libraryfolders.vdf" | sed -E 's/"path"[[:space:]]+"([^"]+)"/\1/')
	fi
	for lib in "${libs[@]}"; do
		if [ -d "$lib/steamapps/compatdata/$APPID" ]; then
			echo "$lib/steamapps/compatdata/$APPID"
			return 0
		fi
	done
	return 1
}
COMPAT_DATA="$(find_compatdata)" || die "Proton prefix for app $APPID not found. Start GTA V once through Steam (with Proton enabled) and try again."
log "Proton prefix: $COMPAT_DATA"

# --- Proton build used for this prefix ----------------------------------------
if [ -z "$PROTON_DIR" ] && [ -f "$COMPAT_DATA/config_info" ]; then
	# config_info: line 1 = Proton version, line 2 = <proton>/(dist|files)/share/fonts/
	fonts_dir="$(sed -n 2p "$COMPAT_DATA/config_info")"
	PROTON_DIR="${fonts_dir%%/share/fonts*}"
	PROTON_DIR="${PROTON_DIR%/dist}"
	PROTON_DIR="${PROTON_DIR%/files}"
fi
[ -n "$PROTON_DIR" ] && [ -x "$PROTON_DIR/proton" ] || die "Proton not found (looked at '${PROTON_DIR:-<unset>}'). Pass --proton /path/to/Proton"
log "Proton: $PROTON_DIR"

# --- start the game -----------------------------------------------------------
game_running() { pgrep -f '[G]TA5\.exe' >/dev/null 2>&1; }

if [ "$LAUNCH" = 1 ] && ! game_running; then
	log "Starting GTA V through Steam (app $APPID)"
	if command -v steam >/dev/null 2>&1; then
		steam -applaunch "$APPID" >/dev/null 2>&1 &
	elif command -v xdg-open >/dev/null 2>&1; then
		xdg-open "steam://run/$APPID" >/dev/null 2>&1 &
	else
		die "Neither 'steam' nor 'xdg-open' found. Start the game yourself and re-run with --no-launch"
	fi
fi

log "Waiting for GTA5.exe (timeout ${TIMEOUT}s)"
waited=0
until game_running; do
	sleep 2
	waited=$((waited + 2))
	[ "$waited" -ge "$TIMEOUT" ] && die "Timed out waiting for GTA5.exe"
done
log "GTA5.exe is running, injecting orange-core.dll"

# --- inject inside the same prefix --------------------------------------------
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$STEAM_ROOT"
export STEAM_COMPAT_DATA_PATH="$COMPAT_DATA"
cd "$CLIENT_DIR"
exec "$PROTON_DIR/proton" run "$CLIENT_DIR/Launcher.exe" --inject --timeout "$TIMEOUT" ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
