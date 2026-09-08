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
#   4. when the launcher exits, shows the tail of launcher.log and client.log
#      and tells you whether orange-core activated inside the game
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
#   --logs             only print the log files of the last run and exit
#   --no-log-tail      do not print the logs after the launcher exits
#   -h, --help         show this help
#
# Environment overrides: ORANGE_CLIENT_DIR, STEAM_ROOT, PROTON_DIR, GTA_APPID
#
# Logs:
#   <client dir>/launcher.log   every step of the launcher (update check,
#                               waiting for GTA5.exe, injection result)
#   <client dir>/client.log     what orange-core.dll does inside the game
#                               (game version, offsets, hooks, network)
#   PROTON_LOG=1 ./gta-orange-proton.sh   additionally writes Wine's output
#                               for the launcher to ~/steam-271590.log; set
#                               "PROTON_LOG=1 %command%" as the game's Steam
#                               launch option to get the same for GTA5.exe
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
LOG_TAIL=1
LOGS_ONLY=0
EXTRA_ARGS=()

usage() { sed -n '2,45p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }
log() { printf '[gta-orange] %s\n' "$*" >&2; }
die() { log "ERROR: $*"; exit 1; }

while [ $# -gt 0 ]; do
	case "$1" in
		--client-dir) CLIENT_DIR="$2"; shift 2 ;;
		--proton) PROTON_DIR="$2"; shift 2 ;;
		--no-launch) LAUNCH=0; shift ;;
		--timeout) TIMEOUT="$2"; shift 2 ;;
		--logs) LOGS_ONLY=1; shift ;;
		--no-log-tail) LOG_TAIL=0; shift ;;
		-h|--help) usage; exit 0 ;;
		--) shift; EXTRA_ARGS+=("$@"); break ;;
		*) EXTRA_ARGS+=("$1"); shift ;;
	esac
done

LAUNCHER_LOG="$CLIENT_DIR/launcher.log"
CLIENT_LOG="$CLIENT_DIR/client.log"

# --- log helpers --------------------------------------------------------------
show_log() {
	local file="$1" lines="${2:-25}"
	echo "----- $file (last $lines lines) -----" >&2
	if [ -f "$file" ]; then
		tail -n "$lines" "$file" >&2
	else
		echo "(not written yet)" >&2
	fi
}

# Reads client.log and explains the outcome of the last injection.
summarize_client_log() {
	[ -f "$CLIENT_LOG" ] || { log "client.log was not written: orange-core.dll did not load inside GTA5.exe (see launcher.log)"; return; }
	local version
	version="$(grep -a 'Game version:' "$CLIENT_LOG" | tail -n 1 | sed -E 's/.*Game version: ([^,]+),.*/\1/')"
	[ -n "$version" ] && log "GTA5.exe version seen by orange-core: $version"
	if grep -aq 'Game patches applied' "$CLIENT_LOG" && [ "$(grep -ac 'Game patches applied' "$CLIENT_LOG")" -ge 1 ]; then
		if tail -n 200 "$CLIENT_LOG" | grep -aq 'stays inactive'; then
			:
		else
			log "orange-core is ACTIVE in the game (patches applied)."
		fi
	fi
	if tail -n 200 "$CLIENT_LOG" | grep -aq 'stays inactive'; then
		log "orange-core stayed INACTIVE: this GTA V build is not supported by the built-in offsets."
		local template
		template="$(ls -t "$CLIENT_DIR"/offsets-*.generated.ini 2>/dev/null | head -n 1 || true)"
		[ -n "$template" ] && log "A template with the missing offsets was written to: $template"
		log "See docs/UPDATING_OFFSETS.md (in the repository) for how to fill in offsets.ini."
	fi
}

# The binaries next to this script must be the ones from the package: the
# auto-updater may have replaced them, or the zip (which contains a client/
# folder) was unpacked inside the client folder. Compares version.txt with
# the version the launcher wrote to launcher.log.
check_package_version() {
	[ -f "$CLIENT_DIR/version.txt" ] && [ -f "$LAUNCHER_LOG" ] || return 0
	local packaged running
	packaged="$(tr -d '[:space:]' < "$CLIENT_DIR/version.txt")"
	running="$(grep -a 'GTA:Orange Launcher .* started' "$LAUNCHER_LOG" | tail -n 1 | sed -E 's/.*GTA:Orange Launcher (.*) started.*/\1/')"
	if [ -n "$packaged" ] && [ -n "$running" ] && [ "$packaged" != "$running" ]; then
		log "WARNING: the launcher that ran is version '$running', but this package is '$packaged'."
		log "         Launcher.exe / orange-core.dll next to this script are not the ones from the package"
		log "         (the auto-updater replaced them, or the zip was unpacked into another folder)."
		log "         Unpack the package over this folder with:  unzip -o -j gta-orange-client-win64.zip -d '$CLIENT_DIR'"
	fi
}

if [ "$LOGS_ONLY" = 1 ]; then
	show_log "$LAUNCHER_LOG" 40
	show_log "$CLIENT_LOG" 60
	summarize_client_log
	check_package_version
	exit 0
fi

[ -f "$CLIENT_DIR/Launcher.exe" ] || die "Launcher.exe not found in '$CLIENT_DIR' (use --client-dir)"
[ -f "$CLIENT_DIR/orange-core.dll" ] || die "orange-core.dll not found in '$CLIENT_DIR'"
if [ -f "$CLIENT_DIR/client/Launcher.exe" ]; then
	log "WARNING: $CLIENT_DIR/client/Launcher.exe exists: the zip was probably unpacked inside the client folder."
	log "         This run uses $CLIENT_DIR/Launcher.exe. To use the freshly unpacked files instead:"
	log "         unzip -o -j gta-orange-client-win64.zip -d '$CLIENT_DIR' && rm -r '$CLIENT_DIR/client'"
fi

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
log "Proton: $PROTON_DIR ($(sed -n 1p "$COMPAT_DATA/config_info" 2>/dev/null || echo 'version unknown'))"
log "Client: $CLIENT_DIR"
log "Logs:   $LAUNCHER_LOG"
log "        $CLIENT_LOG"

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
log "GTA5.exe is running, injecting orange-core.dll (follow along with: tail -f '$LAUNCHER_LOG')"

# --- inject inside the same prefix --------------------------------------------
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$STEAM_ROOT"
export STEAM_COMPAT_DATA_PATH="$COMPAT_DATA"
cd "$CLIENT_DIR"
set +e
"$PROTON_DIR/proton" run "$CLIENT_DIR/Launcher.exe" --inject --timeout "$TIMEOUT" ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
status=$?
set -e

# After a self-update the launcher starts a new copy of itself; wait for it
# too, otherwise the logs below are shown before the injection happened.
waited=0
while pgrep -f 'Launcher\.exe.*--inject' >/dev/null 2>&1 && [ "$waited" -lt 300 ]; do
	[ "$waited" -eq 0 ] && log "The launcher restarted itself after an update, waiting for it to finish"
	sleep 2
	waited=$((waited + 2))
done

if [ "$status" -eq 0 ]; then
	log "Launcher.exe finished (exit code 0)"
else
	log "Launcher.exe exited with code $status"
fi

if [ "$LOG_TAIL" = 1 ]; then
	# orange-core writes client.log from inside the game a moment after injection.
	sleep 3
	show_log "$LAUNCHER_LOG" 25
	show_log "$CLIENT_LOG" 40
	summarize_client_log
	check_package_version
	log "Re-print the logs any time with: $0 --logs"
fi
exit "$status"
