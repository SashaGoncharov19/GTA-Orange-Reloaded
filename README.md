# Grand Theft Auto: Orange

[![Build](https://github.com/SashaGoncharov19/GTA-Orange-Reloaded/actions/workflows/build.yml/badge.svg)](https://github.com/SashaGoncharov19/GTA-Orange-Reloaded/actions/workflows/build.yml)
[![Nightly](https://img.shields.io/badge/download-nightly-orange)](https://github.com/SashaGoncharov19/GTA-Orange-Reloaded/releases/tag/nightly)

GTA:Orange is an alternative multiplayer modification for Grand Theft Auto V
(2016-2017). This repository is a **revival** of the original code base: it
builds again with current toolchains, the dedicated server runs natively on
Linux, the client is built automatically by CI, and the Windows client can be
used with the Steam/Proton version of the game on Linux.

## What is in the box

| Component | Directory | Platforms | Description |
|-----------|-----------|-----------|-------------|
| `orange_server` | `Server/` | Linux, Windows | dedicated server (RakNet networking, built-in HTTP server, YAML config) |
| `lua-module` | `lua-module/` | Linux, Windows | LuaJIT scripting for the server (resources under `resources/`) |
| `simple-module` | `simple-module/` | Linux, Windows | minimal example of a native (C++) server module |
| `OrangeLauncher.exe` | `Launcher/` | Windows (+Proton) | starts GTA V / attaches to it and injects the client core |
| `orange-core.dll` | `orange-core/` | Windows (+Proton) | the client: game hooks, networking, chat/UI (ImGui), client-side Lua |
| `font-converter`, `luajit` | `font-converter/`, `deps/` | all | helper tools |

All third-party libraries are vendored under `deps/` and built from source
(RakNet, tinyxml2, yaml-cpp 0.3, civetweb, LuaJIT 2.1.0-beta2).

## Downloads

Every push to `master` refreshes the rolling
[**nightly** pre-release](https://github.com/SashaGoncharov19/GTA-Orange-Reloaded/releases/tag/nightly);
tags `v*` create versioned releases. Each release contains:

* `gta-orange-server-linux-x64.tar.gz` - dedicated server for Linux (glibc >= 2.35)
* `gta-orange-server-win64.zip` - dedicated server for Windows
* `gta-orange-client-win64.zip` - `OrangeLauncher.exe` + `orange-core.dll` + the Proton helper script
* `client-manifest.txt`, `orange-core.dll`, `OrangeLauncher.exe`, `server-version.txt` - consumed by the auto-updater

The same files are attached as artifacts to every CI run (Actions tab), and
the server is published as a Docker image:
`ghcr.io/sashagoncharov19/gta-orange-reloaded/server:nightly`.

## Quick start

### Server

```bash
# Linux
tar xzf gta-orange-server-linux-x64.tar.gz && cd server
./orange_server                       # type "exit" to stop
# MySQL from Lua (SQLEnv) needs a client library at run time, nothing else does:
#   Debian: sudo apt install libmariadb3    Ubuntu: sudo apt install libmysqlclient21

# Docker
docker run --rm -it -p 7788:7788/udp -p 7789:7789 \
  ghcr.io/sashagoncharov19/gta-orange-reloaded/server:nightly
```

On Windows unzip `gta-orange-server-win64.zip` and run `orange_server.exe`.

`orange_handshake [host] [port] [nickname]` (next to the server) connects
the way the game client does and reports whether the server accepted the
player, without starting the game. Use it to check ports and firewalls from
another machine; the CI smoke tests run it too.

Configuration lives in `config.yml` (name, ports, max players, resources).
Resources are folders under `resources/` with a `resource.yml` and, for Lua
resources, a `main.lua` - see `resources/example/` for a commented example
that spawns cars, handles events and chat commands and answers HTTP requests.
Server events, commands and the whole API exposed to Lua are listed in
`modules/lua-module/API.lua` and `lua-module/SResource.cpp`.

### Client

* **Windows:** unzip `gta-orange-client-win64.zip` anywhere and run
  `OrangeLauncher.exe`. It asks for the GTA V folder once, starts the game and
  injects `orange-core.dll`. `OrangeLauncher.exe --help` lists the options
  (`--inject`, `--game-dir`, `--steam`, `--direct`, `--timeout`, ...).
* **Linux (Steam + Proton):** run `gta-orange-proton.sh` from the client
  folder. It starts GTA V through Steam and injects the client inside the
  game's Proton prefix. Details, requirements and troubleshooting:
  [docs/LINUX_PROTON.md](docs/LINUX_PROTON.md).

Once the game is running with the client, the in-game server browser asks
for a nickname and a server address (default `127.0.0.1:7788`, the server on
the same machine). The address is remembered in `config.xml` next to
`orange-core.dll`. `F12` shows the browser again; the chat (`T`) accepts
`/connect host:port` and `/disconnect`.

### Automatic updates

Nothing has to be downloaded by hand after the first install:

* **Client:** (until 2026-09-08 the executable was `Launcher.exe`; GTA V looks for the Rockstar Games Launcher by that process name, so it was renamed - an old install updates itself and hands over to the new name) every time `OrangeLauncher.exe` starts it fetches
  `client-manifest.txt` from the GitHub releases (`stable` channel = latest
  release, `nightly` = latest `master` build), compares SHA-256 hashes with the
  local `orange-core.dll` / `OrangeLauncher.exe`, downloads what changed, verifies it
  and swaps the files in place (the launcher replaces itself and restarts).
  `gta-orange-proton.sh` refreshes itself and `crossmap_from_fivem.py` from
  the same release (`linux-manifest.txt`).
  A build follows the channel it came from (a nightly client tracks the
  `nightly` pre-release, a release tracks the stable releases) and never
  switches channels on its own; to move an installation run
  `OrangeLauncher.exe --channel nightly --update` (or `--channel stable --update`)
  once. Configure it in `launcher.xml` next to the launcher or with
  `--no-update`, `--update`, `--channel <name>`. Everything is logged to
  `launcher.log`. A failed update check never blocks the game start.
  Development builds (`-dev` version) are left alone unless `--update` is
  given.
* **Server:** run `./update-server.sh` (Linux) or `.\update-server.ps1`
  (Windows) inside the server folder. The scripts compare `version.txt` with
  `server-version.txt` of the release, replace the binaries, the Lua API
  bootstrap and the examples, and leave `config.yml` and `resources/`
  untouched. Add `--channel nightly` / `-Channel nightly` for nightly builds.
  Docker users just pull the new image tag.

> **Game builds:** `orange-core.dll` hooks `GTA5.exe` at ~90 addresses. Only
> nine of them (the script engine) are required and they are found by byte
> patterns, verified on GTA V **1.0.3889.0**; everything else is optional with
> a fallback. On a build where a required entry does not resolve the DLL
> stays inactive instead of crashing the game and writes
> `offsets-<version>.generated.ini` next to itself. Natives are translated
> through `natives-<version>.txt`, which `OrangeLauncher.exe` generates from FiveM's
> public crossmap on first start. See [docs/PORTING_STATUS.md](docs/PORTING_STATUS.md)
> and [docs/UPDATING_OFFSETS.md](docs/UPDATING_OFFSETS.md).

## Building from source

Requirements: CMake >= 3.16, Ninja (or any generator), a C++14 compiler.

```bash
# Linux (server, modules, tools)
sudo apt install build-essential cmake ninja-build libmysqlclient-dev   # MySQL is optional
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DORANGE_BUILD_CLIENT=OFF
cmake --build build --parallel
cmake --install build --prefix dist --component server     # -> dist/server/
```

```powershell
# Windows (server + client) from a "x64 Native Tools Command Prompt for VS 2022"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix dist --component server     # -> dist/server/
cmake --install build --prefix dist --component client     # -> dist/client/
```

A ready-to-run tree is also assembled in `build/bin/{server,client,tools}`
after every build. Useful options:

| Option | Default | Meaning |
|--------|---------|---------|
| `ORANGE_BUILD_SERVER` | `ON` | build `orange_server` and the server modules |
| `ORANGE_BUILD_CLIENT` | `ON` on MSVC, `OFF` elsewhere | build `OrangeLauncher.exe` and `orange-core.dll` |
| `ORANGE_BUILD_TOOLS` | `ON` | build `font-converter` and the `luajit` interpreter |
| `ORANGE_LUA_MYSQL` | `ON` | link lua-module against MySQL/MariaDB client if found (else `_LUA_NOSQL`) |
| `ORANGE_ENABLE_SCALEFORM` | `OFF` | compile the experimental Scaleform DrawText code (needs the proprietary GFx SDK libraries) |
| `ORANGE_BUILD_TESTS` | `ON` | build the unit tests (`ctest --output-on-failure` in the build folder) |
| `ORANGE_VERSION_STRING` | `<version>-dev` | version embedded in the binaries; CI sets the release tag or a nightly id |
| `LUAJIT_ENABLE_GC64` | `OFF` | build LuaJIT in GC64 mode |

The old Visual Studio solution (`Launcher.sln`) is kept for reference but the
CMake build is the supported way to compile the project.

## Continuous integration

`.github/workflows/build.yml` builds the Linux server (inside `ubuntu:22.04`
for wide glibc compatibility) and the Windows server + client (MSVC 2022),
runs the unit tests, smoke-tests both servers (start, HTTP request, log check),
uploads the packages as artifacts, updates the `nightly` release on `master`,
creates releases for `v*` tags and pushes the server Docker image to GHCR.

Every release also carries the files the auto-updater consumes:
`client-manifest.txt` (version + SHA-256 of each client file), the raw
`orange-core.dll` and `OrangeLauncher.exe`, and `server-version.txt`. The version
string embedded in the binaries is the tag (`0.2.0`) for releases and
`nightly-YYYYMMDD-<sha>` for master builds (`-DORANGE_VERSION_STRING=...`).

To publish a release: `git tag v0.3.0 && git push origin v0.3.0`.

## Porting to a current GTA V build

The state of the 1.0.3889.0 port (hooks the FiveM way, build-aware script
thread, natives crossmap generation), what the first live run should show
and what is still missing (structure layouts) are in
[docs/PORTING_STATUS.md](docs/PORTING_STATUS.md); the addresses and
structures found in the game dump are in
[docs/FINDINGS_1.0.3889.0.md](docs/FINDINGS_1.0.3889.0.md).
`OrangeLauncher.exe --dump-game` writes the unpacked game image for a disassembler.

## Known limitations

* **Game build:** the addresses live in one table
  (`orange-core/GameOffsets.cpp`), found by byte patterns and overridable per
  game version through `offsets.ini` (RVA, `disabled`, or a byte pattern).
  1.0.3889.0 resolves everything required; the structure layouts used by the
  synchronisation code (`GTA/CRage.h` and friends) are still the 2017 ones
  and are the open part of the port. The workflow is described in
  [docs/UPDATING_OFFSETS.md](docs/UPDATING_OFFSETS.md).
* **Scaleform:** the DrawText experiment needs Autodesk's GFx 4.0 SDK
  libraries, which are not redistributable; it is disabled by default.
* **MySQL on Windows:** the Windows lua-module is built without LuaSQL/MySQL
  (`_LUA_NOSQL`); the Linux build links against `libmysqlclient`.
* No anti-cheat, no account system, one hard-coded server address in the
  client - this is a 2017 alpha.

## Revival notes (what changed compared to the 2017 code)

* CMake build for all targets and vendored dependencies; LuaJIT is built from
  source through the same steps as its Makefile (`cmake/LuaJIT.cmake`). The
  vendored LuaJIT tree was completed with the upstream `v2.1.0-beta2` build
  files that were missing.
* Server: portable logging/time functions, fixed-size wire types on Linux
  (`DWORD` is 32-bit everywhere), a shared `shared/ModuleAPI.h` so the server
  and all modules agree on the module ABI, optional Lua callbacks no longer
  crash the server, clean shutdown on `exit`/SIGTERM and headless operation
  when stdin is closed (Docker/systemd).
* Client: the developer HWID whitelist that refused to load the DLL on any
  other machine was removed; a game build signature check was added;
  `OrangeLauncher.exe` got command line options, timeouts and error messages
  instead of busy loops.
* Repository: committed build artefacts (CMake caches, object files,
  prebuilt libraries, Code::Blocks files) were removed.

## Credits

Original GTA:Orange team: emcifuntik (Eugene Pogrebnyak), VadZz, frontface,
Yauhen Pahrabniak and everyone listed in the project history. Third-party
components (RakNet, LuaJIT, tinyxml2, yaml-cpp, civetweb, ImGui, MinHook,
Scaleform headers) are covered by their respective licenses in `deps/` and
`orange-core/`.
