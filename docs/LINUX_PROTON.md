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
* The client package `gta-orange-client-win64.zip` (releases page) unpacked
  somewhere, e.g. `~/gta-orange/client`.
* `bash`, `pgrep` and either the `steam` command or `xdg-open`.

### Run

```bash
cd ~/gta-orange/client
chmod +x gta-orange-proton.sh
./gta-orange-proton.sh
```

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
start (see `launcher.xml`; `--no-update` disables it, `--channel nightly`
follows the master builds). The download uses WinHTTP inside the Proton
prefix; if it fails (no network in the prefix, missing TLS support) the
launcher logs the reason to `launcher.log` and starts the game anyway. In that
case update the client folder by hand from the releases page.

The Linux server has its own updater: `./update-server.sh` in the server
folder (`--channel nightly` for nightly builds).

### Where things are logged

* `launcher.log` next to `Launcher.exe` – launcher and auto-updater activity.
* `client.log` next to `orange-core.dll` – everything the client core does,
  including the **game build check** (see below).
* Proton/Wine output – run the script from a terminal; add `WINEDEBUG=+loaddll`
  to the environment to see DLL loading problems.

### Server address

The in-game "Server browser" window currently connects to the address in
`orange-core.dll`'s defaults (`127.0.0.1:7788`). Put an empty file called
`orange.developer` next to `orange-core.dll` to get the *direct connect* fields
(IP and port) in that window.

## 3. Known limitation: supported GTA V build

`orange-core.dll` hooks the game through **hard-coded offsets** into
`GTA5.exe` that were taken from the game build current in **January 2017**
(the last commit of the original project). GTA V has been updated many times
since, and every update moves those offsets.

To avoid crashing the game, the DLL now verifies a set of byte signatures at
the expected offsets before patching anything (`VerifyGameBuild()` in
`orange-core/orange-core.cpp`). On a different build it logs

```
[Error] Signature mismatch: ForceToSingle at GTA5.exe+0x2773c
...
[Error] Game build check failed, GTA:Orange stays inactive
```

shows a message box and stays inactive. Porting the client to a current game
build means replacing those offsets by pattern scans (the comments in
`orange-core.cpp`, `Core/scrEngine.cpp` and `ScaleformManager.h` contain the
original signatures) – this is the main open task for anyone who wants to play
with the mod on a modern GTA V. The build/CI/Proton plumbing in this
repository is ready for that work.
