# GTA:Orange on Linux (native server + Proton client)

GTA:Orange has two halves:

| Part | What it is | Linux status |
|------|------------|--------------|
| `orange_server` + `modules/lua-module.so` | dedicated server, Lua scripting | **native Linux build** (also available as a Docker image) |
| `Launcher.exe` + `orange-core.dll` | game client, injected into `GTA5.exe` | Windows binaries, run through **Proton** with the Steam version of GTA V |

## 1. Server

Download `gta-orange-server-linux-x64.tar.gz` from the
[releases page](https://github.com/SashaGoncharov19/GTA-Orange-Reloaded/releases)
(the `nightly` pre-release always has the latest master build) or build it
yourself (see the README).

```bash
tar xzf gta-orange-server-linux-x64.tar.gz
cd server
# lua-module.so links against the MySQL client library:
sudo apt install libmysqlclient21      # Debian/Ubuntu
./orange_server
```

Ports: **7788/udp** (game) and **7789/tcp** (built-in HTTP server, used by
resources via `OnHTTPReq`). Both are set in `config.yml`.

The server runs its resources from `resources/<name>/` (see
`resources/example/main.lua`) and reads commands from the terminal (`exit`
stops it). When stdin is not a terminal (systemd, Docker) it keeps running
until it receives `SIGINT`/`SIGTERM`.

### Docker

```bash
docker run --rm -it -p 7788:7788/udp -p 7789:7789 \
  ghcr.io/sashagoncharov19/gta-orange-reloaded/server:nightly
```

Mount your own `config.yml` / `resources` over `/opt/gta-orange/server/...`
to customise it.

## 2. Client under Proton

### Requirements

* Steam + GTA V (Steam version) installed and started **at least once** with
  Proton, so that the prefix `steamapps/compatdata/271590` exists.
* The client package `gta-orange-client-win64.zip` (releases page). The
  archive contains a `client/` folder, so unpack it in the parent folder:
  `mkdir -p ~/gta-orange && cd ~/gta-orange && unzip gta-orange-client-win64.zip`
  gives `~/gta-orange/client`. To refresh an existing folder in place use
  `unzip -o -j gta-orange-client-win64.zip -d ~/gta-orange/client`.
* `bash`, `pgrep` and either the `steam` command or `xdg-open`.

### Run

```bash
cd ~/gta-orange/client
chmod +x gta-orange-proton.sh
./gta-orange-proton.sh
```

The script compares `version.txt` of the package with the launcher version in
`launcher.log` and warns when the binaries next to it are not the ones from
the package.

The script

1. locates Steam, the game's Proton prefix and the Proton version Steam uses
   for it,
2. starts GTA V through Steam (`steam -applaunch 271590`),
3. waits until `GTA5.exe` is running and then executes
   `Launcher.exe --inject` **inside the game's prefix** with the same Proton
   build, so the launcher can see the game process and inject
   `orange-core.dll` with the usual `CreateRemoteThread`/`LoadLibrary`
   technique (which Wine supports).

Useful variations:

```bash
./gta-orange-proton.sh --no-launch          # game already running, only inject
./gta-orange-proton.sh --proton ~/.steam/root/compatibilitytools.d/GE-Proton9-27
PROTON_DIR=... STEAM_ROOT=... ./gta-orange-proton.sh
protontricks-launch --appid 271590 ./Launcher.exe --inject   # alternative
```

`Launcher.exe --help` lists all launcher options (`--inject`, `--game-dir`,
`--timeout`, `--no-unpack-wait`, ...).

### Automatic updates under Proton

`Launcher.exe` checks the GitHub releases for a newer client before every
start (see `launcher.xml`; `--no-update` disables it). A nightly client
follows the `nightly` pre-release and a release the stable releases; the
launcher never switches channels on its own, so a nightly client is not
"updated" to the older stable release. To switch, run once:

```bash
./gta-orange-proton.sh --no-launch --channel stable --update    # or --channel nightly
```

(unknown options of the script are passed on to `Launcher.exe`). The download
uses WinHTTP inside the Proton prefix; if it fails (no network in the prefix,
missing TLS support) the launcher logs the reason to `launcher.log` and starts
the game anyway. In that case update the client folder by hand from the
releases page.

The Linux server has its own updater: `./update-server.sh` in the server
folder (`--channel nightly` for nightly builds).

### Where things are logged

The script prints the paths before injecting and shows the tail of both logs
when the launcher exits (`./gta-orange-proton.sh --logs` re-prints them any
time):

* `launcher.log` next to `Launcher.exe` – every launcher step: options,
  update check, the game folder, waiting for `GTA5.exe` (pid), the unpack
  wait, the injection result (`LoadLibrary returned 0x...` = the DLL is loaded
  in the game).
* `client.log` next to `orange-core.dll` – everything the client core does
  inside the game: its version, the `GTA5.exe` path and version, the
  **offset resolution** (one line per entry, see below), hooks, network.
* Proton/Wine output – `PROTON_LOG=1 ./gta-orange-proton.sh` writes Wine's
  log for the launcher to `~/steam-271590.log`; for the game itself set
  `PROTON_LOG=1 %command%` as the launch option of GTA V in Steam. Add
  `WINEDEBUG=+loaddll` to see DLL loading problems.

How to read a run:

| You see | Meaning |
|---|---|
| `launcher.log`: `inject: FAILED: OpenProcess failed` | the launcher does not run in the same Proton prefix as the game (use the script, not a different Proton/prefix) |
| `launcher.log`: `LoadLibrary failed inside the game process` | `orange-core.dll` could not be loaded - a missing dependency, or the DLL is not the x64 build; `WINEDEBUG=+loaddll` tells which |
| `launcher.log`: `done: orange-core.dll injected`, no `client.log` | the DLL loaded but could not write its log (folder not writable?) |
| `client.log`: `Game patches applied` | orange-core is active, F7 opens the chat in game |
| `client.log`: `Game build check failed, GTA:Orange stays inactive` | unsupported game build - see the next section |

### Server address

The in-game "Server browser" window currently connects to the address in
`orange-core.dll`'s defaults (`127.0.0.1:7788`). Put an empty file called
`orange.developer` next to `orange-core.dll` to get the *direct connect* fields
(IP and port) in that window.

## 3. Known limitation: supported GTA V build

`orange-core.dll` hooks the game at about 90 addresses inside `GTA5.exe`. The
built-in values were taken from the game build current in **January 2017**
(the last commit of the original project); every game update moves them.

To avoid crashing the game, the DLL first identifies the build (five byte
signatures at their reference addresses) and resolves every address it needs
(`GameOffsets::Initialize()` in `orange-core/GameOffsets.cpp`). On a different
build `client.log` shows

```
[Info] Game version: 1.0.3411.0, image base 0x7FF6C0A20000, image size 0x4A5B000
[Info] Game build check: NOT the reference build, offsets must come from offsets.ini or pattern scans
[Error] offset CodeCave: UNRESOLVED (required) - no pattern known for this entry
...
[Info] Offsets: 90 total, 0 reference, 0 from offsets.ini, 14 by pattern, 0 disabled, 76 unresolved (18 required)
[Info] Offsets template written to Z:\home\you\gta-orange\client\offsets-1.0.3411.0.generated.ini
[Error] Game build check failed, GTA:Orange stays inactive
```

shows a message box and stays inactive. The generated
`offsets-<version>.generated.ini` lists every entry with a description; the
addresses for your build go into `offsets.ini` next to the DLL. The full
workflow (syntax, how to find each kind of address, what else changes between
builds) is in [UPDATING_OFFSETS.md](UPDATING_OFFSETS.md). The build/CI/Proton
plumbing in this repository is ready for that work.
