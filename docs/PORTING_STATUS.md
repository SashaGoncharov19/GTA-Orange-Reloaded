# GTA:Orange revival: status and the road to a current GTA V build

Written 2026-09-08 after the first real run on GTA V **1.0.3889.0** (Steam,
Linux + Proton). This is the hand-over document: what exists, what the first
run showed, what is still missing, and what to look for. Everything referenced
here is in this repository.

## 1. What is done

| Area | State |
|---|---|
| Build | CMake for everything (server, Lua module, client, tools); vendored RakNet / tinyxml2 / yaml-cpp / civetweb / LuaJIT 2.1.0-beta2 built from source. Linux (GCC) and Windows (MSVC 2022+). |
| Server | Native Linux build, headless-safe (SIGINT/SIGTERM), IPv6 fallback, MySQL optional. Docker image `ghcr.io/sashagoncharov19/gta-orange-reloaded/server`. |
| CI/CD | `.github/workflows/build.yml`: every push builds Linux + Windows, runs unit tests and a server smoke test; `master` refreshes the rolling **`nightly`** pre-release; `v*` tags or a manual run publish releases. |
| Downloads | `gta-orange-client-win64.zip`, `gta-orange-server-win64.zip`, `gta-orange-server-linux-x64.tar.gz` plus the files the updaters use. |
| Auto-update | `Launcher.exe` updates `orange-core.dll` and itself from GitHub releases (SHA-256 verified, self-replace + restart). A build follows the channel it came from (nightly / stable) and never switches channel on its own. `update-server.sh` / `.ps1` do the same for servers. |
| Proton | `tools/proton/gta-orange-proton.sh`: finds Steam, the game's prefix and Proton, starts the game, runs `Launcher.exe --inject` inside the prefix, shows both logs afterwards and says whether orange-core activated. |
| Diagnostics | `launcher.log` records every launcher step (update, game folder, pid, unpack wait, injection result, anti-cheat module warning). `client.log` records everything orange-core does inside the game, including the offset resolution. |
| Game offsets | One table (`orange-core/GameOffsets.cpp`) with the 90 addresses orange-core needs, each with a name, the reference RVA (January 2017 build), optional byte pattern candidates and a required/optional flag. Resolution order: `offsets.ini [version]` → `[default]` → reference RVA (only on the reference build) → pattern scan → unresolved. Unresolved optional entries are skipped safely; unresolved required entries keep the mod inactive. A template `offsets-<version>.generated.ini` is written on every unsupported build. |
| Game dump | `Launcher.exe --dump-game` (or `./gta-orange-proton.sh --dump-game`) writes the unpacked in-memory `GTA5.exe` to `GTA5-<version>.dump.exe` next to the launcher, section table fixed so IDA / Ghidra open it and file offsets equal RVAs. |

Pull requests, in order: #1 revival, #2 offsets table + launcher logging,
#3 LF line endings, #4 updater channels, #5 package check in the script,
#6 self-update restart + unpack detection, #7 pattern candidates + FiveM
alternates, #8 this document + `--dump-game`.

## 2. What the first run on 1.0.3889.0 showed

`launcher.log`: the client updated itself on the nightly channel, injected
into `GTA5.exe` (pid found, `LoadLibrary` succeeded). No anti-cheat module
was reported. `client.log`: orange-core loaded, read version **1.0.3889.0**
(image size `0x3E5BC00`), found it is **not** the reference build, resolved
what it could and stayed inactive. The game kept running, the message box
named 25 unknown required offsets (23 after PR #8: `CanLangChange` and
`InitHUD` have safe fallbacks and became optional).

Built-in patterns that matched on 1.0.3889.0 (values are RVAs):

| Entry | RVA | Note |
|---|---|---|
| ForceToSingle | 0x2A730 | pattern from the original authors |
| ForceToSingle_2 | 0x2CA3E0 | pattern from the original authors |
| ScriptHandlerMgr | 0xA5ACBC | FiveM-style pattern |
| ScriptThreadKill | 0xA5AA04 | FiveM-style pattern |
| UnknownPatch_1 | 0x2D1D14 | optional |
| UnknownPatch_3 | 0x2E2EAC | optional |

PR #7 added FiveM's current alternates for `ScrThreadCollection`,
`ActiveThreadTlsOffset`, `ScrThreadId`, `ScrThreadCount`, `GetScriptIdBlock`
and (PR #8) `RegistrationTable`. Whether they match 1.0.3889.0 is not known
yet: run the current nightly once and read `client.log`.

## 3. The three layers a port needs

Resolving offsets is necessary but **not sufficient**. Three things changed
between January 2017 and 1.0.3889.0:

1. **Addresses** (this table). Patterns cover the script engine; the rest
   needs a disassembler.
2. **Native hashes.** Rockstar reshuffles the hashes natives are registered
   under with every build, and since 1.0.1290 the registration table itself is
   obfuscated. orange-core now handles both: `Natives.h` calls every native by
   its **canonical** hash (the one every database uses, e.g.
   https://docs.fivem.net/natives), `Core/NativeTable.cpp` translates it to
   the running build and walks the plain or the obfuscated table. The
   translation comes from a built-in table on the reference build and from
   **`natives-<game version>.txt`** next to `orange-core.dll` on any other
   build (`0x<canonical> 0x<build hash> [name]` per line). That file is the
   missing piece for 1.0.3889.0: FiveM's copy is not public, ScriptHookV's is
   inside a closed DLL. On every non-reference build orange-core writes
   `natives-<version>.registered.txt` (build hash + handler RVA of every native
   the game registered, once `RegistrationTable` resolves) as raw material.
3. **Structure layouts.** `GTA/CRage.h`, `Core/scrThread.h`, entity pools
   (`CReplayInterface.h`), the task serialisation in `Network/*` read game
   structures by fixed member offsets from 2017.

## 4. Route: the architecture stays (no ScriptHookV)

Decision: keep orange-core's own script thread, native invocation and hooks;
no ScriptHookV dependency. That means every layer of section 3 has to be
reversed for 1.0.3889.0 and again for every future build. The plumbing for
it is in place; the data is what has to be produced.

### Natives: producing `natives-1.0.3889.0.txt`

Needed: the pairs canonical hash → 1.0.3889.0 hash for the natives orange-core
uses (5179 in `Natives.h`, the full list with canonical hashes is
`orange-core/Core/NativeCrossmap_Reference.h`; one native still lacks its
canonical hash, see `tools/natives/natives-without-canonical.txt`).

Ways to get them, none of which needs ScriptHookV:

1. **A per-build natives table that already exists.** Script decompilers for
   GTA V ship a natives table per game build (they need it to name the natives
   in decompiled `.ysc` scripts); repositories of decompiled scripts for build
   3889 (or the closest build) carry it. Convert it to the file format above.
2. **Script diffing.** The game's own scripts (`update.rpf`, `.ysc`) call
   natives by the hash of their build. The same script decompiled from a build
   with a known table and from 3889 aligns almost one to one; aligned call
   sites give hash pairs. Tooling: CodeWalker / a `.ysc` decompiler.
3. **Handler matching.** `natives-1.0.3889.0.registered.txt` lists every
   registered hash with the RVA of its handler; the same list from a build with
   a known table (or the handlers' code) can be matched function by function
   (BinDiff-style). Handlers of simple natives are near-identical between
   builds.

A partial file is fine: untranslated natives are reported once in
`client.log` and their calls do nothing.

### Registration table

`NativeTable.cpp` decodes the obfuscated layout the way FiveM does (next
pointer, count and hashes XOR-ed with their own address). It is selected for
every build ≥ 1.0.1290 that is not the reference build. If
`natives-<version>.registered.txt` stays empty although `RegistrationTable`
resolved, the layout changed again and the struct in `NativeTable.cpp` needs
updating.

### What stays reversed by hand

Section 5 (addresses) and the structure layouts (`GTA/CRage.h`,
`Core/scrThread.h`, `GTA/CReplayInterface.h`, `Network/*` task sync). The
alternative route through the ScriptHookV SDK, which removes most of this,
was considered and rejected; it is kept in the git history of this document
for reference.

## 5. If the 2017 architecture is kept: what to find

Work on the dump (`--dump-game`), addresses are RVAs. Test with
`offsets.ini`, see section 6. Required first:

| Entry | What it is | How to identify it |
|---|---|---|
| `ScrThreadCollection`, `ActiveThreadTlsOffset`, `ScrThreadId`, `ScrThreadCount`, `RegistrationTable`, `GetScriptIdBlock`, `ScriptThreadTick`, `ScriptThreadInit` | script engine globals / functions | first check `client.log` for the FiveM candidates; otherwise take the newest patterns from FiveM `code/components/rage-scripting-five/src/scrEngine.cpp` / `scrThread.cpp` and convert (FiveM points at the rel32, we point at the instruction: our delta = theirs − opcode length) |
| `CodeCave` | 48 bytes of unused executable memory | any run of ≥ 48 padding bytes (`CC`/`00`) at the end of `.text` |
| `WindowCreateCall` | the `FF 15` call to `CreateWindowExW` that creates the game window | xrefs to `__imp_CreateWindowExW`, the call whose class name is `grcWindow` |
| `LookAliveCall` | `E8` call executed once per frame (game main loop) | the function called from the main loop that runs `CGame::Update`-like work; or move the per-frame work into the script thread and set the entry to `disabled` |
| `GameStateChangeCall` | `E8` call where the game state machine changes state (values: 0 playing, 1 intro, 3 licence, 5 main menu, 6 loading SP/MP) | the function that writes the state; or poll via natives |
| `ShutdownLoadingScreen`, `DoScreenFadeIn`, `HasScriptLoaded`, `TerminateAllScriptsWithThisName`, `ForceCleanupForAllThreadsWithThisName` | C++ implementations behind natives of the same name | the native's handler in the registration table (hash from docs.fivem.net/natives) calls the implementation |
| `ReplayInterfaces` | `ReplayInterfaces*` global (pools of peds, vehicles, objects, pickups) | widely used by SP trainers as "ReplayInterface"; xrefs to the vtable construction |
| `World` | `CWorld*` global (local ped, player info) | SP trainer sources ("World" / "CWorld" / "getLocalPed") |
| `ViewportGame` | `CViewportGame*` global (view/projection matrices) | SP trainer sources ("CViewPort", "WorldToScreen") |
| `SwapChain` | global holding the game's `IDXGISwapChain*` | xrefs to the `CreateSwapChain` call result; or hook `Present` through a dummy device's vtable and set the entry to `disabled` |
| `GetEntityFromScriptHandle`, `GetEntityAddressCall` | script handle → entity pointer | `fwScriptGuid::GetBaseFromGuid` ("getScriptHandleBaseAddress") in SP mod sources |

Optional entries (`GameProcessHooks`, `UnknownPatch_*`, task sync, allocator)
can stay unresolved; each one just switches off one patch or one feature.
The descriptions in `offsets-<version>.generated.ini` say what each patch
does, so a replacement by natives is usually possible.

## 6. Testing candidates

1. Put candidates into `offsets.ini` next to `orange-core.dll`:

   ```ini
   [1.0.3889.0]
   ScrThreadCollection = 48 8B C8 EB ? 33 C9 48 8B 05 @ 7 | 48 8B C8 EB 03 49 8B CD 48 8B 05 @ 8
   CodeCave            = 0x2E4F80
   EscFreeze           = disabled
   ```

   `0x...` = RVA, `disabled` = skip, `scan` = built-in patterns, `bytes @ delta`
   = own pattern, `|` separates candidates (first unique match wins,
   ambiguous ones are refused).
2. Run `./gta-orange-proton.sh`; then read `client.log`: one line per entry
   (`offset X = 0x... (offsets.ini [1.0.3889.0] pattern #2 of 3)` or
   `UNRESOLVED ... 2 pattern(s) not found`) and the summary line.
3. This is safe: orange-core stays inactive until **every** required entry
   is resolved, so wrong candidates cannot crash the game. Only when the
   summary says `0 unresolved required` does it patch; put an empty
   `orange.developer` file next to the DLL to force patching earlier (expect
   crashes, useful to test one hook at a time).

## 7. Hand-back checklist

To finish the port from here, send back:

* the `offsets.ini` `[1.0.3889.0]` section with what resolved, plus
  `client.log` of the run;
* `natives-1.0.3889.0.txt` (section 4), even partial, and the
  `natives-1.0.3889.0.registered.txt` orange-core wrote;
* the structure offsets that changed (`CPed`, `CVehicle`, `CPlayerInfo`,
  `CViewportGame`, `ReplayInterfaces`, `scrThread`), ideally as a diff of
  the headers under `orange-core/GTA` and `orange-core/Core`.
