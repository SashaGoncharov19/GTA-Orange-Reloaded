#!/usr/bin/env bash
#
# update-server.sh - update a GTA:Orange Linux server installation in place
# from the GitHub releases.
#
# Usage: ./update-server.sh [--dir DIR] [--channel stable|nightly] [--force] [--repository owner/repo]
#
#   --dir DIR        server folder (default: the folder of this script)
#   --channel NAME   stable = latest release, nightly = latest master build
#                    (default: the channel the installed build came from, so a
#                    nightly server is never downgraded to the stable release)
#   --force          reinstall even if the version already matches
#
# Binaries, the Lua API bootstrap, the examples and this script are replaced;
# config.yml and your resources/ are left untouched. Restart the server
# afterwards.

set -euo pipefail

REPO="${ORANGE_REPOSITORY:-SashaGoncharov19/GTA-Orange-Reloaded}"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHANNEL=""
FORCE=0

while [ $# -gt 0 ]; do
	case "$1" in
		--dir) DIR="$2"; shift 2 ;;
		--channel) CHANNEL="$2"; shift 2 ;;
		--repository) REPO="$2"; shift 2 ;;
		--force) FORCE=1; shift ;;
		-h|--help) sed -n '2,16p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
		*) echo "unknown option: $1" >&2; exit 2 ;;
	esac
done

LOCAL="$(cat "$DIR/version.txt" 2>/dev/null | tr -d '[:space:]' || true)"
if [ -z "$CHANNEL" ]; then
	case "$LOCAL" in
		nightly-*) CHANNEL="nightly" ;;
		*) CHANNEL="stable" ;;
	esac
	echo "[update] channel not given, using the channel of the installed build: $CHANNEL"
fi

if [ "$CHANNEL" = "nightly" ]; then
	BASE="https://github.com/$REPO/releases/download/nightly"
else
	BASE="https://github.com/$REPO/releases/latest/download"
fi

command -v curl >/dev/null 2>&1 || { echo "curl is required" >&2; exit 1; }

echo "[update] channel: $CHANNEL ($BASE)"
REMOTE="$(curl -fsSL "$BASE/server-version.txt" | tr -d '[:space:]')"
echo "[update] installed: ${LOCAL:-unknown}, available: $REMOTE"

if [ "$REMOTE" = "$LOCAL" ] && [ "$FORCE" = 0 ]; then
	echo "[update] already up to date"
	exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
echo "[update] downloading gta-orange-server-linux-x64.tar.gz"
curl -fSL --progress-bar -o "$TMP/server.tar.gz" "$BASE/gta-orange-server-linux-x64.tar.gz"
tar -xzf "$TMP/server.tar.gz" -C "$TMP"
SRC="$TMP/server"

install -m 755 "$SRC/orange_server" "$DIR/orange_server"
mkdir -p "$DIR/modules/lua-module" "$DIR/resources"
install -m 755 "$SRC/modules/lua-module.so" "$DIR/modules/lua-module.so"
cp "$SRC/modules/lua-module/API.lua" "$DIR/modules/lua-module/API.lua"
rm -rf "$DIR/examples" && cp -r "$SRC/examples" "$DIR/examples"
[ -e "$DIR/config.yml" ] || cp "$SRC/config.yml" "$DIR/config.yml"
[ -e "$DIR/resources/example" ] || cp -r "$SRC/resources/example" "$DIR/resources/example"
cp "$SRC/version.txt" "$DIR/version.txt"
[ -e "$SRC/update-server.sh" ] && install -m 755 "$SRC/update-server.sh" "$DIR/update-server.sh"

echo "[update] updated ${LOCAL:-unknown} -> $REMOTE. Restart the server to apply."
