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
| `Launcher.exe` | `Launcher/` | Windows (+Proton) | starts GTA V / attaches to it and injects the client core |
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
* `gta-orange-client-win64.zip` - `Launcher.exe` + `orange-core.dll` + the Proton helper script
* `client-manifest.txt`, `orange-core.dll`, `Launcher.exe`, `server-version.txt` - consumed by the auto-updater

The same files are attached as artifacts to every CI run (Actions tab), and
the server is published as a Docker image:
`ghcr.io/sashagoncharov19/gta-orange-reloaded/server:nightly`.

## Quick start

### Server

```bash
# Linux
tar xzf gta-orange-server-linux-x64.tar.gz && cd server
sudo apt install libmysqlclient21     # needed by modules/lua-module.so
./orange_server                       # type "exit" to stop

# Docker
docker run --rm -it -p 7788:7788/udp -p 7789:7789 \
  ghcr.io/sashagoncharov19/gta-orange-reloaded/server:nightly
```

On Windows unzip `gta-orange-server-win64.zip` and run `orange_server.exe`.

Configuration lives in `config.yml` (name, ports, max players, resources).
Resources are folders under `resources/` with a `resource.yml` and, for Lua
resources, a `main.lua` - see `resources/example/` for a commented example
that spawns cars, handles events and chat commands and answers HTTP requests.
Server events, commands and the whole API exposed to Lua are listed in
`modules/lua-module/API.lua` and `lua-module/SResource.cpp`.

### Client

* **Windows:** unzip `gta-orange-client-win64.zip` anywhere and run
  `Launcher.exe`. It asks for the GTA V folder once, starts the game and
  injects `orange-core.dll`. `Launcher.exe --help` lists the options
  (`--inject`, `--game-dir`, `--steam`, `--direct`, `--timeout`, ...).
* **Linux (Steam + Proton):** run `gta-orange-proton.sh` from the client
  folder. It starts GTA V through Steam and injects the client inside the
  game's Proton prefix. Details, requirements and troubleshooting:
  [docs/LINUX_PROTON.md](docs/LINUX_PROTON.md).

The client connects to the server address configured in
`orange-core.dll` (default `127.0.0.1:7788`); an empty `orange.developer` file
next to the DLL enables the direct-connect fields in the in-game server
browser.

### Automatic updates

Nothing has to be downloaded by hand after the first install:

* **Client:** every time `Launcher.exe` starts it fetches
  `client-manifest.txt` from the GitHub releases (`stable` channel = latest
  release, `nightly` = latest `master` build), compares SHA-256 hashes with the
  local `orange-core.dll` / `Launcher.exe`, downloads what changed, verifies it
  and swaps the files in place (the launcher replaces itself and restarts).
  Configure it in `launcher.xml` next to the launcher or with `--no-update`,
  `--update`, `--channel nightly`. Everything is logged to `launcher.log`.
  A failed update check never blocks the game start. Development builds
  (`-dev` version) are left alone unless `--update` is given.
* **Server:** run `./update-server.sh` (Linux) or `.\update-server.ps1`
  (Windows) inside the server folder. The scripts compare `version.txt` with
  `server-version.txt` of the release, replace the binaries, the Lua API
  bootstrap and the examples, and leave `config.yml` and `resources/`
  untouched. Add `--channel nightly` / `-Channel nightly` for nightly builds.
  Docker users just pull the new image tag.

> **Important:** `orange-core.dll` hooks `GTA5.exe` through hard-coded offsets
> from the **January 2017** game build. On any other build it now detects the
> mismatch, logs it to `client.log` and stays inactive instead of crashing the
> game. Porting the hooks to a current GTA V build (pattern scanning) is the
> main open task - see [Known limitations](#known-limitations).

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
| `ORANGE_BUILD_CLIENT` | `ON` on MSVC, `OFF` elsewhere | build `Launcher.exe` and `orange-core.dll` |
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
`orange-core.dll` and `Launcher.exe`, and `server-version.txt`. The version
string embedded in the binaries is the tag (`0.2.0`) for releases and
`nightly-YYYYMMDD-<sha>` for master builds (`-DORANGE_VERSION_STRING=...`).

To publish a release: `git tag v0.3.0 && git push origin v0.3.0`.

## Known limitations

* **Game build:** the client targets the GTA V build of January 2017. The
  offsets in `orange-core/orange-core.cpp`, `orange-core/Core/scrEngine.cpp`
  and `orange-core/ScaleformManager.h` (the original signatures are kept in
  comments) have to be replaced by pattern scans before the client can work
  with a current game version. Until then the DLL refuses to patch an unknown
  build.
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
  `Launcher.exe` got command line options, timeouts and error messages
  instead of busy loops.
* Repository: committed build artefacts (CMake caches, object files,
  prebuilt libraries, Code::Blocks files) were removed.

## Credits

Original GTA:Orange team: emcifuntik (Eugene Pogrebnyak), VadZz, frontface,
Yauhen Pahrabniak and everyone listed in the project history. Third-party
components (RakNet, LuaJIT, tinyxml2, yaml-cpp, civetweb, ImGui, MinHook,
Scaleform headers) are covered by their respective licenses in `deps/` and
`orange-core/`.
