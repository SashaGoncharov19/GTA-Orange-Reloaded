# GTA:Orange revival: status and the road to a current GTA V build

Updated 2026-09-08 after analysing the memory dump of GTA V **1.0.3889.0**
(Steam, Linux + Proton). This is the hand-over document: what exists, what the
dump showed, what the code now does on that build and what is still missing.
The verified addresses and structures are in `docs/FINDINGS_1.0.3889.0.md`;
how offsets are resolved is in `docs/UPDATING_OFFSETS.md`.

## 1. What is done

| Area | State |
|---|---|
| Build | CMake for everything (server, Lua module, client, tools); vendored RakNet / tinyxml2 / yaml-cpp / civetweb / LuaJIT 2.1.0-beta2 built from source. Linux (GCC) and Windows (MSVC 2022+). |
| Server | Native Linux build, headless-safe (SIGINT/SIGTERM), IPv6 fallback, MySQL optional. Docker image `ghcr.io/sashagoncharov19/gta-orange-reloaded/server`. |
| CI/CD | `.github/workflows/build.yml`: every push builds Linux + Windows, runs unit tests and a server smoke test; `master` refreshes the rolling **`nightly`** pre-release; `v*` tags or a manual run publish releases. |
| Downloads | `gta-orange-client-win64.zip`, `gta-orange-server-win64.zip`, `gta-orange-server-linux-x64.tar.gz` plus the files the updaters use. |
| Auto-update | `OrangeLauncher.exe` updates `orange-core.dll` and itself from GitHub releases (SHA-256 verified, self-replace + restart). A build follows the channel it came from (nightly / stable) and never switches channel on its own. `update-server.sh` / `.ps1` do the same for servers. |
| Proton | `tools/proton/gta-orange-proton.sh`: finds Steam, the game's prefix and Proton, starts the game, runs `OrangeLauncher.exe --inject` inside the prefix, shows both logs afterwards and says whether orange-core activated. |
| Diagnostics | `launcher.log` records every launcher step (update, game folder, pid, unpack wait, natives crossmap, injection result, anti-cheat module warning). `client.log` records everything orange-core does inside the game: offset resolution, every hook, the script thread, the natives translation. |
| Game offsets | One table (`orange-core/GameOffsets.cpp`, 92 entries) with a name, the reference RVA (January 2017 build), byte pattern candidates and a required/optional flag per entry. Resolution order: `offsets.ini [version]` → `[default]` → reference RVA (reference build only) → pattern scan → unresolved. **Only 9 entries are required** (the script engine globals and functions), all of them found by pattern on 1.0.3889.0; everything else is optional and has a fallback. |
| Hooks | The FiveM approach (`rage-scripting-five`), with MinHook: the game's "start the startup script" function triggers GTA:Orange's initialisation, `GtaThread::Tick` lets the game boot with its own scripts and, once the player is in the world, runs only GTA:Orange's threads (the single player scripts are frozen from then on), the script id comparison answers "same script", the window message pump is the per-frame hook. Present is hooked through the vtable of a temporary swap chain, the game window comes from the swap chain. The reference build's call-site patches (code cave) remain as fallbacks. |
| Script thread | The thread object is allocated with a large zeroed tail and reads the script handler position from the game's own Kill code (`+0x110` on the reference build, `+0x118` since 1.0.2699), vtable slot 5 is reserved for `CacheThreadData` (1.0.3570+). |
| Start-up | Two causes of "failed to initialize, error code 1005" (the Social Club SDK) were found and removed. `GTA5.exe` looks for the Rockstar Games Launcher by process name, `Launcher.exe`, which was our launcher's name: it is `OrangeLauncher.exe` now (old installs are handed over by the auto-updated binary). And something touching `GTA5.exe` during its first seconds (the unpack-detection memory reads alone, no DLL loaded) upsets the same SDK. The launcher therefore injects only after the game window has existed for 45 s (`--inject-after`), the way the one working session was injected by accident. `orange-core` then lets the game boot with its own scripts and takes over once the player is in the world (`ScriptEngine::TakeOver`), so the `startup` script the game waits for does run. |
| Natives | `Natives.h` calls natives by canonical hash; `Core/NativeTable.cpp` translates through `natives-<version>.txt` and walks the obfuscated registration table (1.0.1290+). **`natives-<version>.txt` is generated automatically** from FiveM's public `CrossMapping_Universal.h`: by `gta-orange-proton.sh` before injecting (Linux side, reads the version out of `GTA5.exe`) and by `OrangeLauncher.exe` from inside the prefix. The launcher falls back to reading the game version from the running process, because under Proton the game's own path is a drive mapping it usually cannot open. The obfuscated registration record is decoded the way the game does it (hash entries from +0x54, `shared/NativeRegistrationObf.h`, unit-tested); the first decoder read +0x58 and every native answered zero without a word - `nativeCall` now reports a missing handler once per native. |
| Game dump | `OrangeLauncher.exe --dump-game` (or `./gta-orange-proton.sh --dump-game`) writes the unpacked in-memory `GTA5.exe` to `GTA5-<version>.dump.exe`, section table fixed so file offsets equal RVAs. |

Pull requests, in order: #1 revival, #2 offsets table + launcher logging,
#3 LF line endings, #4 updater channels, #5 package check in the script,
#6 self-update restart + unpack detection, #7 pattern candidates + FiveM
alternates, #8 `--dump-game` + the first version of this document,
#9 canonical native hashes + crossmap file + obfuscated table, #10 (this
change) the 1.0.3889.0 port: verified patterns, FiveM-style hooks, build
aware thread layout, natives crossmap generation.

## 2. What the dump of 1.0.3889.0 showed

Everything is written down in `docs/FINDINGS_1.0.3889.0.md`. In short:

* every script engine entry resolves by pattern (thread collection, TLS
  offset, thread id / count, registration table, script handler manager,
  Tick / Kill / Init); the FiveM patterns for 3258+ match;
* the startup-script function, the message pump, the window creation, the
  entity-from-handle function and the local player ped global were found and
  have patterns now;
* the script thread object grew to 0x160 bytes and its script handler moved
  from `+0x110` to `+0x118` (the +4 script hash added in 1.0.2699);
* the natives crossmap problem is solved by FiveM's public universal table
  (column 27 serves every build since 1.0.2944; 6482 of 6494 entries are
  registered on 3889, the 219 natives added later keep their canonical hash);
* not found: the game state change call, the viewport, the swap chain global,
  the replay interfaces (entity pools) and the five native implementations
  the reference build called directly. None of them is needed any more.

## 3. What happens on 1.0.3889.0 now

**Confirmed live on 2026-09-08** (Steam, Linux + Proton, injected into a
running story-mode game with `./gta-orange-proton.sh --no-launch --
--inject-after 5`): every required offset resolves by pattern, the hooks
install, the game accepts and ticks the script thread, 6441 translations and
6701 registered natives, natives execute, rendering and input work on the
game's 1920x1080 swap chain, and the client scripts run: the player is
teleported to the lobby camera scene, becomes `mp_m_freemode_01`, the chat
and the server browser appear. Connecting to a server is the next step
(the 2017 lobby has no server list; the address is typed in the browser or
with `/connect`, section 4).

`client.log` should read like this on a good run (abridged):

```
[Info] Game version: 1.0.3889.0, image base ..., image size 0x3E5BC00
[Info] Game build check: NOT the reference build, offsets must come from offsets.ini or pattern scans
[Debug] offset ScrThreadCollection = 0xA4BCEE (built-in pattern #1 of 3)
...
[Info] Offsets: 92 total, 0 reference, 0 from offsets.ini, 23 by pattern, 0 disabled, 69 unresolved (0 required)
[Info] Game patches applied
[Info] Hook LookAlive: installed at 0x...
[Info] Hook StartupScript: installed at 0x...
[Info] Hook ScriptThreadTick: installed at 0x...
[Info] Script hooks: the stock scripts run until the game has booted, then only GTA:Orange threads run
[Info] Natives: 6430 translation(s) from ...\natives-1.0.3889.0.txt
[Info] StartupScript hook: the game is about to start its startup script
[Info] Game ready: initialising GTA:Orange
[Info] ScriptEngine: script handler at thread+0x118 (read from ScriptThreadKill)
[Info] D3DHook: SwapChain offset unresolved, probing DXGI for IDXGISwapChain::Present
[Info] Hook IDXGISwapChain::Present: installed at 0x...
[Info] Created script thread, id N in slot M
[Info] D3DHook: rendering initialised (1920x1080)
[Info] Input hook attached: WndProc 0x...
[Info] ScriptThread: script handler attached to thread N
```

The flow:

1. `DllMain` resolves the offsets, applies the byte patches that resolved
   (`ForceToSingle`, `UnknownPatch_1`, `UnknownPatch_3` match on 3889; all
   `GameProcessHooks` patches have no pattern and are skipped) and redirects
   the `CreateWindowExW` call site (window title and icon).
2. A worker thread installs the MinHook hooks: message pump (per frame),
   startup script (game ready), thread tick (stock scripts frozen after boot), script
   id comparison.
3. When the game starts its `startup` script, GTA:Orange initialises the
   script engine, hooks Present, subclasses the window, creates its script
   thread and starts the client scripts. Without a natives crossmap the
   scripts are not started and the chat says why.
4. The script thread runs every frame through the Tick hook and calls natives
   through the crossmap; the stock scripts never run, so the loading screen
   has to be left by GTA:Orange itself (`SHUTDOWN_LOADING_SCREEN`,
   `DO_SCREEN_FADE_IN` from the client scripts).

Two files next to `orange-core.dll` change the behaviour: `orange.developer`
(apply patches even with unresolved required entries, extra logging,
developer UI) and `orange.storymode` (let the game's own scripts run; the
Tick and script id hooks pass everything through).

## 4. What is still missing

1. **Structure layouts** (the third layer). `GTA/CRage.h` (`CPed`,
   `CVehicle`, `CPlayerInfo`, `CViewportGame`), `GTA/CReplayInterface.h`,
   `GTA/VTasks.*` and the task serialisation in `Network/*` are the 2017
   layouts. Everything that reads them directly (on-foot sync data such as
   `CWorld::Get()->CPedPtr->MoveSpeed`, aim data, task trees, pool walks)
   reads wrong fields on 3889. The safe path is to route these through
   natives where one exists and to verify the rest against the dump the way
   the thread object was verified (see the findings document, section 3).
   Public references for current layouts: FiveM's headers, the
   `gtav-classes` repository, SP mod menus.
2. **The first live run: done** (section 3). What it showed: the 2017
   request for the `standard_global_init` script threw inside the game's
   native (removed, nothing used it); the lobby is the 2017 one (teleport to
   a camera scene over Vinewood, freemode ped, server browser) and looks
   whatever the story-mode time of day makes it look; the server browser
   pointed at a hardcoded "beta-test server" (now: the address is typed in
   the browser or with `/connect host:port`, remembered in `config.xml`,
   and every connection step is logged as `Network: ...`).
3. **The first connected session.** Run `orange_server` (Linux build) next
   to the game and connect to `127.0.0.1:7788`. Expected on 3889: the
   server accepts the player and the `example` resource teleports it to its
   spawn, creates vehicles, a blip and a marker; remote players appear as
   peds but move by teleport-interpolation only (`CNetworkPlayer::AssignTask`
   is refused off the reference build); the local player's on-foot data is
   read through natives where the 2017 structure reads were replaced, and
   the remaining structure reads (aim, tasks) are wrong until item 1 is done.
4. `ReplayInterfaces`, `ViewportGame` and the gameplay patches
   (`GameProcessHooks`) have no patterns; only the debug pool overlay and the
   cosmetic patches depend on them.

## 5. Natives crossmap: how it works and how to redo it

* The file `natives-<version>.txt` (`0x<canonical> 0x<build hash> [name]`
  per line, `shared/NativeCrossmap.h`) lives next to `orange-core.dll`.
* `OrangeLauncher.exe` creates it before injecting when it is missing: it reads
  the game version from `GTA5.exe`, downloads
  `code/components/rage-scripting-five/include/CrossMapping_Universal.h`
  from `github.com/citizenfx/fivem` and converts the column for the build
  (`shared/NativeCrossmapUniversal.h`, unit test
  `tests/universal_crossmap_test.cpp`). `launcher.log` reports the result.
* By hand (adds native names and checks against the natives the game
  registered):

  ```bash
  python3 tools/natives/crossmap_from_fivem.py --version 1.0.3889.0 \
      --registered natives-1.0.3889.0.registered.txt --out natives-1.0.3889.0.txt
  ```

* The table is not committed to this repository: `rage-scripting-five` is
  outside the LGPL-covered part of FiveM's tree (Cfx.re / Take-Two terms).
  Regenerating on the user's machine keeps the project clear of
  redistributing it. `.gitignore` excludes the generated files.
* Natives added after 1.0.2944 keep their canonical hash; orange-core uses
  the canonical hash whenever no translation exists, so they work without a
  line in the file. `natives-<version>.registered.txt` (written by
  orange-core once the game is ready, on every non-reference build) lists what
  the game registers. It is written at that point and not at injection time
  because the registration table is still empty while the first loading screen
  runs, which is when the launcher injects.

## 6. Testing candidates for a build that does not resolve

1. Put candidates into `offsets.ini` next to `orange-core.dll`:

   ```ini
   [1.0.4000.0]
   ScrThreadCollection = 48 8B C8 EB ? 33 C9 48 8B 05 @ 7 | 48 8B C8 EB 03 49 8B CD 48 8B 05 @ 8
   StartupScript       = 0x2E4F80
   ForceToSingle       = disabled
   ```

   `0x...` = RVA, `disabled` = skip, `scan` = built-in patterns, `bytes @ delta`
   = own pattern, `|` separates candidates (first unique match wins,
   ambiguous ones are refused).
2. Run `./gta-orange-proton.sh`; then read `client.log`: one line per entry
   (`offset X = 0x... (offsets.ini [1.0.4000.0] pattern #2 of 3)` or
   `UNRESOLVED ... 2 pattern(s) not found`) and the summary line.
3. This is safe: orange-core stays inactive until **every** required entry
   is resolved, so wrong candidates cannot crash the game. Only when the
   summary says `0 unresolved required` does it patch; put an empty
   `orange.developer` file next to the DLL to force patching earlier (expect
   crashes, useful to test one hook at a time).

## 7. Hand-back checklist

To finish the port from here, send back:

* `client.log` and `launcher.log` of a run on the nightly build;
* the `offsets-<version>.generated.ini` orange-core wrote (it shows what
  resolved and what did not);
* if the game crashed: the last lines of `client.log` and, if there is one,
  the crash address (Proton prints it in the terminal);
* the structure offsets that changed (`CPed`, `CVehicle`, `CPlayerInfo`,
  `CViewportGame`, `ReplayInterfaces`), ideally as a diff of the headers under
  `orange-core/GTA`.
