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
#         OrangeLauncher.exe --inject
#      inside the very same Proton prefix, which injects orange-core.dll into
#      the running game
#   4. when the launcher exits, shows the tail of launcher.log and client.log
#      and tells you whether orange-core activated inside the game
#
# Usage:
#   ./gta-orange-proton.sh [options] [-- extra OrangeLauncher.exe options]
#
# Options:
#   --client-dir DIR   folder with OrangeLauncher.exe + orange-core.dll
#                      (default: the folder of this script)
#   --proton DIR       Proton installation to use (default: the one Steam
#                      configured for GTA V, read from compatdata/config_info)
#   --no-launch        do not start the game, only wait for GTA5.exe and inject
#   --timeout SEC      how long to wait for GTA5.exe (default 600)
#   --logs             only print the log files of the last run and exit
#   --no-log-tail      do not print the logs after the launcher exits
#   --inject-after SEC passed to OrangeLauncher.exe: wait for the game window and
#                      SEC more seconds before injecting (default 45). The
#                      game's Social Club SDK initialises during its first
#                      seconds and reports "error code 1005" when disturbed;
#                      0 injects right after the executable is unpacked.
#   --dump-game        passed to OrangeLauncher.exe: write the unpacked GTA5.exe
#                      image (for IDA / Ghidra) next to this script, no inject
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
#   protontricks-launch --appid 271590 /path/to/OrangeLauncher.exe --inject

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

# What the auto-updater did on the last run. The launcher replaces itself and
# restarts, so "up to date" on the second start is the normal, successful end
# of an update; without this the whole thing is invisible in the output.
summarize_update() {
	[ -f "$LAUNCHER_LOG" ] || return 0
	local running downloaded remote
	running="$(grep -a 'Launcher .* starting' "$LAUNCHER_LOG" | tail -n 1 | sed -E 's/.*Launcher ([^ ]+) starting.*/\1/' || true)"
	downloaded="$(grep -ac 'updater: downloading' "$LAUNCHER_LOG" || true)"
	remote="$(grep -a 'updater: local version' "$LAUNCHER_LOG" | tail -n 1 | sed -E 's/.*remote version ([^,]+).*/\1/' || true)"
	[ -n "$running" ] && log "Client version: $running"
	if grep -aq 'just restarted after a self-update' "$LAUNCHER_LOG"; then
		log "Auto-update: the client updated itself and restarted (now on $running)"
	elif tail -n 60 "$LAUNCHER_LOG" | grep -aq 'updater: up to date'; then
		log "Auto-update: already the newest ${remote:+$remote }build, nothing to download"
	elif tail -n 60 "$LAUNCHER_LOG" | grep -aq 'updater: .*skipped'; then
		log "Auto-update: skipped this run (see launcher.log for why)"
	elif [ "${downloaded:-0}" -gt 0 ]; then
		log "Auto-update: downloaded $downloaded file(s)"
	fi
	local crossmap
	crossmap="$(ls -t "$CLIENT_DIR"/natives-*.txt 2>/dev/null | grep -v '\.registered\.txt$' | head -n 1 || true)"
	if [ -n "$crossmap" ]; then
		log "Natives crossmap: $(basename "$crossmap") ($(grep -avc '^;' "$crossmap" || echo 0) translations)"
	else
		log "Natives crossmap: MISSING - natives cannot be called, scripts will not start (see client.log)"
	fi
}

# Reads client.log and explains the outcome of the last injection.
#
# client.log is appended to, so it holds every run orange-core ever made in
# this folder. Judging the whole file (or a blind tail of it) reports an old
# failed run as the current state, so everything below looks at the last run
# only: from the last "orange-core <version> loaded from ..." banner to the end.
summarize_client_log() {
	local marker
	[ -f "$CLIENT_LOG" ] || { log "client.log was not written: orange-core.dll did not load inside GTA5.exe (see launcher.log)"; return; }

	# The launcher says so when the DLL was already in the process: then the
	# game was never re-entered and client.log below is from an earlier run.
	if [ -f "$LAUNCHER_LOG" ] && awk '/---- GTA:Orange Launcher .* started ----/ { block = "" } { block = block $0 "\n" } END { printf "%s", block }' "$LAUNCHER_LOG" | grep -aq 'is ALREADY loaded'; then
		log "This run injected nothing: orange-core.dll was already loaded in the running GTA5.exe."
		log "         Windows hands back the module that is already there without running it again."
		log "         Restart GTA V (not just this script) to load the current orange-core.dll."
	fi

	for marker in orange.nohooks orange.storymode orange.developer; do
		[ -f "$CLIENT_DIR/$marker" ] || continue
		case "$marker" in
			orange.storymode) log "NOTE: orange.storymode is present: the single player scripts keep running (HUD, missions, story)."
			                  log "      Delete it for the normal behaviour, where only GTA:Orange runs once the game has booted." ;;
			orange.nohooks)   log "NOTE: orange.nohooks is present: orange-core resolves the offsets and does nothing else." ;;
			orange.developer) log "NOTE: orange.developer is present (developer mode: relaxed build check, debug commands)." ;;
		esac
	done

	local run
	run="$(awk '/orange-core .* loaded from/ { block = "" } { block = block $0 "\n" } END { printf "%s", block }' "$CLIENT_LOG")"
	[ -n "$run" ] || run="$(cat "$CLIENT_LOG")"

	local version started
	started="$(printf '%s' "$run" | grep -a 'loaded from' | tail -n 1 | sed -E 's/^\[([^]]*)\].*/\1/' || true)"
	version="$(printf '%s' "$run" | grep -a 'Game version:' | tail -n 1 | sed -E 's/.*Game version: ([^,]+),.*/\1/' || true)"
	[ -n "$started" ] && log "Last orange-core run: $started"
	[ -n "$version" ] && log "GTA5.exe version seen by orange-core: $version"

	if printf '%s' "$run" | grep -aq 'stays inactive'; then
		log "orange-core stayed INACTIVE: this GTA V build is not supported by the built-in offsets."
		local template
		template="$(ls -t "$CLIENT_DIR"/offsets-*.generated.ini 2>/dev/null | head -n 1 || true)"
		[ -n "$template" ] && log "A template with the missing offsets was written to: $template"
		log "See docs/UPDATING_OFFSETS.md (in the repository) for how to fill in offsets.ini."
		return
	fi

	printf '%s' "$run" | grep -aq 'Game patches applied' && log "orange-core is ACTIVE in the game (patches applied)."
	printf '%s' "$run" | grep -aq 'Game hooks installed' && log "Hooks installed: $(printf '%s' "$run" | grep -ac 'Hook .*: installed') of them."
	if printf '%s' "$run" | grep -aq 'Game ready: done'; then
		log "The game reached 'ready' and GTA:Orange initialised."
	else
		log "The game has not reached 'ready' yet in this run (still loading, or the trigger never fired)."
	fi
	local translations
	translations="$(printf '%s' "$run" | grep -a 'Natives: .* translation' | tail -n 1 | sed -E 's/.*Natives: ([0-9]+) translation.*/\1/' || true)"
	if [ -n "$translations" ] && [ "$translations" != "0" ]; then
		log "Natives: $translations translations loaded."
	elif printf '%s' "$run" | grep -aq 'no crossmap for game version'; then
		log "Natives: NO crossmap loaded, so no script was started (see the crossmap line above)."
	fi

	local exceptions missing
	exceptions="$(printf '%s' "$run" | grep -ac 'exception inside native' || true)"
	missing="$(printf '%s' "$run" | grep -ac 'Natives: no handler for' || true)"
	[ "${exceptions:-0}" != "0" ] && log "Natives: $exceptions call(s) threw inside the game ('exception inside native' lines in client.log)."
	[ "${missing:-0}" != "0" ] && log "Natives: $missing native(s) have no handler in this build ('no handler for' lines in client.log)."

	# The connection: orange-core logs every step of it since 2026-09-08.
	local config net
	config="$(printf '%s' "$run" | grep -a 'Config: server' | tail -n 1 | sed -E 's/.*Config: //' || true)"
	[ -n "$config" ] && log "Client config: $config"
	if printf '%s' "$run" | grep -aq 'Network: the server accepted the player'; then
		log "Connected: the server accepted the player, the game is synchronised with it."
	elif printf '%s' "$run" | grep -aq 'Network: connection accepted by'; then
		log "Connected to the server; the player was not accepted yet (or the run ended before)."
	elif printf '%s' "$run" | grep -aq 'Network: no answer from'; then
		net="$(printf '%s' "$run" | grep -a 'Network: no answer from' | tail -n 1 | sed -E 's/.*Network: //' || true)"
		log "Not connected: $net"
		log "         Start the server first (docs/LINUX_PROTON.md, section 1), then press Connect again"
		log "         (F12 opens the server browser, or type /connect host:port in the chat)."
	elif printf '%s' "$run" | grep -aq 'Network: connecting to'; then
		net="$(printf '%s' "$run" | grep -a 'Network: connecting to' | tail -n 1 | sed -E 's/.*Network: //' || true)"
		log "Network: $net, no answer recorded (yet)."
	elif printf '%s' "$run" | grep -aq 'Game ready: done'; then
		log "No connection was attempted in this run (the server browser: nickname, address, Connect)."
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
	running="$(grep -a 'GTA:Orange Launcher .* started' "$LAUNCHER_LOG" | tail -n 1 | sed -E 's/.*GTA:Orange Launcher (.*) started.*/\1/' || true)"
	if [ -n "$packaged" ] && [ -n "$running" ] && [ "$packaged" != "$running" ]; then
		log "WARNING: the launcher that ran is version '$running', but this package is '$packaged'."
		log "         OrangeLauncher.exe / orange-core.dll next to this script are not the ones from the package"
		log "         (the auto-updater replaced them, or the zip was unpacked into another folder)."
		log "         Unpack the package over this folder with:  unzip -o -j gta-orange-client-win64.zip -d '$CLIENT_DIR'"
	fi
}

if [ "$LOGS_ONLY" = 1 ]; then
	show_log "$LAUNCHER_LOG" 40
	show_log "$CLIENT_LOG" 60
	summarize_update
	summarize_client_log
	check_package_version
	exit 0
fi

# GTA5.exe looks for the Rockstar Games Launcher by process name, and that
# name is Launcher.exe: with a launcher of ours running under it the game's
# Social Club fails to initialise (error code 1005). Hence OrangeLauncher.exe.
LAUNCHER_EXE="$CLIENT_DIR/OrangeLauncher.exe"
if [ ! -f "$LAUNCHER_EXE" ]; then
	if [ -f "$CLIENT_DIR/Launcher.exe" ]; then
		LAUNCHER_EXE="$CLIENT_DIR/Launcher.exe"
		log "WARNING: only the old Launcher.exe is here. GTA5.exe mistakes a process of that name for the Rockstar"
		log "         Games Launcher and fails with error 1005. It will try to hand over to OrangeLauncher.exe once"
		log "         the auto-updater has fetched it; unpacking the current client package fixes it for good."
	else
		die "OrangeLauncher.exe not found in '$CLIENT_DIR' (use --client-dir)"
	fi
fi
[ -f "$CLIENT_DIR/orange-core.dll" ] || die "orange-core.dll not found in '$CLIENT_DIR'"
if [ -f "$CLIENT_DIR/client/OrangeLauncher.exe" ] || [ -f "$CLIENT_DIR/client/Launcher.exe" ]; then
	log "WARNING: $CLIENT_DIR/client/OrangeLauncher.exe exists: the zip was probably unpacked inside the client folder."
	log "         This run uses $CLIENT_DIR/OrangeLauncher.exe. To use the freshly unpacked files instead:"
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
# Every Steam library folder: the root plus the ones libraryfolders.vdf lists.
steam_libraries() {
	local lib
	echo "$STEAM_ROOT"
	if [ -f "$STEAM_ROOT/steamapps/libraryfolders.vdf" ]; then
		grep -oE '"path"[[:space:]]+"[^"]+"' "$STEAM_ROOT/steamapps/libraryfolders.vdf" \
			| sed -E 's/"path"[[:space:]]+"([^"]+)"/\1/'
	fi
}

find_compatdata() {
	local lib
	while IFS= read -r lib; do
		if [ -d "$lib/steamapps/compatdata/$APPID" ]; then
			echo "$lib/steamapps/compatdata/$APPID"
			return 0
		fi
	done < <(steam_libraries)
	return 1
}

# GTA5.exe on the Linux side (the launcher only ever sees it through the
# prefix's drive mapping, which it cannot always open).
find_game_exe() {
	local lib
	while IFS= read -r lib; do
		if [ -f "$lib/steamapps/common/Grand Theft Auto V/GTA5.exe" ]; then
			echo "$lib/steamapps/common/Grand Theft Auto V/GTA5.exe"
			return 0
		fi
	done < <(steam_libraries)
	return 1
}

# orange-core translates every native it calls to the hash the running build
# registers, through natives-<game version>.txt next to orange-core.dll.
# OrangeLauncher.exe downloads that table itself, but only when it can read the game
# version, which needs the game file - so generate it here, where GTA5.exe is
# a plain Linux path. Never fatal: without it orange-core still loads and says
# what is missing.
ensure_natives_crossmap() {
	local exe tool candidate
	exe="$(find_game_exe)" || { log "Natives crossmap: GTA5.exe not found in any Steam library, leaving it to OrangeLauncher.exe"; return 0; }
	tool=""
	for candidate in "$CLIENT_DIR/crossmap_from_fivem.py" \
	                 "$(dirname "${BASH_SOURCE[0]}")/../natives/crossmap_from_fivem.py"; do
		if [ -f "$candidate" ]; then
			tool="$candidate"
			break
		fi
	done
	if [ -z "$tool" ]; then
		log "Natives crossmap: crossmap_from_fivem.py not found next to this script, leaving it to OrangeLauncher.exe"
		return 0
	fi
	if ! command -v python3 >/dev/null 2>&1; then
		log "Natives crossmap: python3 not installed, leaving it to OrangeLauncher.exe"
		return 0
	fi
	log "Natives crossmap: checking (game: $exe)"
	local output status
	set +e
	output="$( cd "$CLIENT_DIR" && python3 "$tool" --exe "$exe" --skip-existing 2>&1 )"
	status=$?
	set -e
	while IFS= read -r line; do
		[ -n "$line" ] && log "  $line"
	done <<< "$output"
	[ "$status" -eq 0 ] || log "Natives crossmap: generation failed (no internet?), leaving it to OrangeLauncher.exe"
	return 0
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
ensure_natives_crossmap

log "GTA5.exe is running, injecting orange-core.dll (follow along with: tail -f '$LAUNCHER_LOG')"

# --- inject inside the same prefix --------------------------------------------
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$STEAM_ROOT"
export STEAM_COMPAT_DATA_PATH="$COMPAT_DATA"
cd "$CLIENT_DIR"
set +e
"$PROTON_DIR/proton" run "$LAUNCHER_EXE" --inject --timeout "$TIMEOUT" ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
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
	log "OrangeLauncher.exe finished (exit code 0)"
else
	log "OrangeLauncher.exe exited with code $status"
fi

if [ "$LOG_TAIL" = 1 ]; then
	# orange-core writes client.log from inside the game a moment after injection.
	sleep 3
	show_log "$LAUNCHER_LOG" 25
	show_log "$CLIENT_LOG" 40
	summarize_update
	summarize_client_log
	check_package_version
	log "Re-print the logs any time with: $0 --logs"
fi
exit "$status"
