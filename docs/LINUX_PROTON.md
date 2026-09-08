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

### "Failed to initialize. Error code 1005"

That message comes from the game's Social Club SDK, which initialises during
the first seconds after `GTA5.exe` starts and reports 1005 when it cannot
complete ([Rockstar Support](https://support.rockstargames.com/articles/mYBXIDmxRAvgk1rQwoGI2/rockstar-games-error-code-1005)).
It appeared on every fresh start while the launcher watched the game process
from its first second (a full-access handle plus memory reads for the unpack
detection), including a run where the injection itself failed and no DLL was
ever loaded, and never when the game had been running for a while before the
launcher touched it.

So the launcher now leaves the process alone until the game window exists and
a further 45 seconds have passed (`--inject-after SEC`, `0` restores the old
"right after unpack" behaviour). `launcher.log` shows `wait for window: ...`
lines during that time. If 1005 still appears with the launcher not running at
all (start the game through Steam alone to check), it is between the game and
Rockstar's servers: verify the game files in Steam, quit Steam completely and
start it again, and make sure nothing blocks the Rockstar Games Launcher.

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

## 3. Supported GTA V build

`orange-core.dll` hooks the game at about 90 addresses inside `GTA5.exe`.
Only nine of them (the script engine) are required; they are found by byte
patterns that were verified on GTA V **1.0.3889.0** (`docs/FINDINGS_1.0.3889.0.md`).
Everything else is optional with a fallback, so the DLL activates on that
build. On a build where a required entry does not resolve, `client.log`
shows

```
[Info] Game version: 1.0.4000.0, image base 0x7FF6C0A20000, image size 0x4A5B000
[Info] Game build check: NOT the reference build, offsets must come from offsets.ini or pattern scans
[Error] offset ScrThreadCollection: UNRESOLVED (required) - built-in 3 pattern(s) not found
...
[Info] Offsets: 92 total, 0 reference, 0 from offsets.ini, 20 by pattern, 0 disabled, 72 unresolved (1 required)
[Info] Offsets template written to Z:\home\you\gta-orange\client\offsets-1.0.4000.0.generated.ini
[Error] Game build check failed, GTA:Orange stays inactive
```

shows a message box and stays inactive. The generated
`offsets-<version>.generated.ini` lists every entry with a description; the
addresses for your build go into `offsets.ini` next to the DLL. The full
workflow (syntax, how to find each kind of address, what else changes between
builds) is in [UPDATING_OFFSETS.md](UPDATING_OFFSETS.md).

Natives are called by their canonical hash and translated through
`natives-<version>.txt` next to the DLL. Two things produce it, and the first
one that succeeds wins:

1. `gta-orange-proton.sh` generates it before injecting: it finds `GTA5.exe`
   in your Steam libraries, reads the game version straight out of the
   executable and runs `crossmap_from_fivem.py` (shipped in the client
   package). Needs `python3` and internet on the Linux side. The run prints
   `Natives crossmap: natives-<version>.txt (N translations)`.
2. `Launcher.exe` does the same from inside the prefix when the file is still
   missing (`launcher.log` shows `natives: wrote N translation(s)`).

The launcher reads the game version from the running process when it cannot
open the game file itself, which is the normal case under Proton: the game's
own path is a drive mapping (`S:\...`) that the launcher process often cannot
open. To build the file by hand:

```bash
python3 crossmap_from_fivem.py --exe "$HOME/.steam/debian-installation/steamapps/common/Grand Theft Auto V/GTA5.exe"
```

Without that file orange-core loads, patches the game and installs its hooks,
but does not start any script, because every native call would go to a hash
this build does not register. `client.log` and the script output both say so.

What the client still cannot do on a current build is described in
`docs/PORTING_STATUS.md` (section 4): the structure layouts used by the
synchronisation code are the 2017 ones.
