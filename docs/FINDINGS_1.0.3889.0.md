# GTA V 1.0.3889.0: what was found in the dump

Written 2026-09-08 from the memory dump `GTA5-1.0.3889.0.dump.exe`
(65,387,520 bytes, produced by `OrangeLauncher.exe --dump-game`, section table fixed
so that file offsets equal RVAs) plus the agent report on branch
`porting-report-3889`. Every value below is an RVA (address minus the image
base `0x140000000`), verified with `objdump` and the `.pdata` function table
unless marked otherwise. This is the raw material behind the code changes in
`orange-core`; `docs/PORTING_STATUS.md` says what to do with it.

## 1. The executable

| Item | Value |
|---|---|
| Version resource | 1.0.3889.0 (Steam, Legacy edition: `GTA5.exe`, D3D11, imports `d3d9.dll`/`DINPUT8.dll`/`VERSION.dll`, no `d3d12.dll`) |
| Build string (FiveM `TableBuilder.cpp`) | `Jul  9 2026` = FiveM `xbr::Build::Summer_2026 = 3889` |
| SizeOfImage | `0x3E5BC00`, section alignment `0x1000` |
| `.text` | `0x1000` .. `0x19CE600` (VirtualSize `0x19CD600`), characteristics `0x60000020` |
| second `.text` | `0x3188000` .. `0x3E5BC00` (Arxan / packer code) |
| `.rdata` | `0x19D1000` .. `0x1D19600`; `.data` `0x1D1A000` .. `0x2FC8088`; `.pdata` `0x2FC9000` (function table used for the checks below) |
| Code cave | the page slack after `.text`: `0x19CE600` .. `0x19CF000`, 2560 zero bytes, executable (same page as the section) |

## 2. Offsets that resolve (pattern verified unique on this dump)

The patterns are the ones now built into `orange-core/GameOffsets.cpp`.
"delta" is added to the match address; the value is what orange-core uses.

| Entry | RVA | Pattern (`@ delta`) | Evidence |
|---|---|---|---|
| ForceToSingle | `0x2A730` | `48 83 EC 28 85 D2 78 71 75 0F @ 0` | function start (.pdata); dispatcher on `edx` 0..4; the `E9` at +0x3A is the jump patched by orange-core |
| ForceToSingle_2 | `0x2CA3E0` | `48 83 EC 28 B9 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? B1 01 @ 0` | function start; calls f(6), g(4), h(1) then a state check |
| UnknownPatch_1 | `0x2D1D14` | `48 85 C9 0F 84 ? 00 00 00 48 8D 55 A7 E8 @ 0` | inside function `0x2D1BF0`, the `E8` at +13 exists |
| UnknownPatch_3 | `0x2E2EAC` | `48 89 5C 24 ? 57 48 83 EC 20 8B F9 8B DA @ 0` | function start |
| UnknownPatch_2 | - | `E8 ? ? ? ? 8B CB 40 88 2D ? ? ? ?` | no match on this build (patch skipped) |
| ScrThreadCollection | `0xA4BCEE` | `48 8B C8 EB ? 33 C9 48 8B 05 @ 7` (FiveM 3258+) | `mov rax,[rip+X]` -> global `0x2E9DC30` (GtaThread** array), count is the WORD at `0x2E9DC38` |
| ActiveThreadTlsOffset | `0x16754E1` | `48 8B 04 D0 4A 8B 14 00 48 8B 01 F3 44 0F 2C 42 20 @ -4` | dword value **`0x2A48`** |
| ScrThreadId | `0x16759B9` | `8B 15 ? ? ? ? 48 8B 05 ? ? ? ? FF C2 89 15 ? ? ? ? 48 8B 0C F8 @ 0` (FiveM 3258+) | `mov edx,[rip+X]` -> global `0x2E9A740`; the code increments it and stores it into `thread+8` (context.ThreadId) |
| ScrThreadCount | `0x1676D93` | `FF 0D ? ? ? ? 48 8B D9 75 @ 0` (FiveM 2545+) | `dec dword [rip+X]` -> global `0x2E9B608` |
| RegistrationTable | `0x1678A81` | `76 32 48 8B 53 40 @ 6` | `lea rcx,[rip+X]` -> global `0x2E9A750` (256 buckets); the function resolves a program's native list and substitutes the stub `0x1675E64` for unknown natives |
| ScriptHandlerMgr | `0xA5ACBC` | `74 17 48 8B C8 E8 ? ? ? ? 48 8D 0D @ 10` | `lea rcx,[rip+X]` -> object `0x2604DF0`; its vtable `0x1AAF430`: slot 10 = `0xA5D4F8` (AttachScript), slot 11 = `0xA63B7C` (DetachScript, called by Kill) |
| ScriptThreadTick | `0xA64738` | `80 B9 ? 01 00 00 00 8B FA 48 8B D9 74 05 @ -15` | function start; `cmp byte [rcx+0x14E],0` (paused flag), state read at `[rcx+0x10]` |
| ScriptThreadKill | `0xA5AA04` | `48 83 EC 20 48 83 B9 ? 01 00 00 00 48 8B D9 74 14 @ -6` | function start; `cmp qword [rcx+0x118],0` then `scriptHandlerMgr->DetachScript(thread)`; calls Init at the end |
| ScriptThreadInit | `0xA58E70` | `83 89 ? 01 00 00 FF 83 A1 ? 01 00 00 F0 @ 0` (FiveM, delta 0) | leaf function: writes `+0x118 +0x120 +0x12C +0x134 +0x13C +0x140(-1) +0x144 +0x148 +0x14C +0x150 +0x152 +0x153 +0x158 +0x15C` |
| GetEntityFromScriptHandle | `0x167E3A4` | `83 F9 FF 74 ? 8B D1 C1 FA 08 85 D2 78 ? 4C 8B 05 ? ? ? ? 41 3B 50 10 @ 0` | `fwScriptGuid::GetBaseFromGuid`: handle -> pool `0x2E9E4D8` -> `[entry+8]`; 28 call sites |
| World | `0x709BFC` | `48 8B 05 ? ? ? ? 48 8B 40 08 C3 @ 0` | getter `mov rax,[rip+X]; mov rax,[rax+8]; ret` -> global `0x2503A58`, `+8` = local player ped |
| WindowCreateCall | `0x1410223` | `4C 8B C1 8B CE FF 15 ? ? ? ? 41 8B D4 48 8B C8 48 8B D8 FF 15 @ 5` | `call [0x1937C5C]` = `USER32!CreateWindowExW`, followed by ShowWindow / UpdateWindow |
| LookAlive (new) | `0x141069C` | `40 55 53 56 57 41 57 48 8D 6C 24 C9 48 81 EC 90 00 00 00 8B 05 @ 0` | the per-frame window message pump: one-time `ntdll!RtlGetDeviceFamilyInfoEnum` check, `SetThreadExecutionState(3)`, `PeekMessageW` / `TranslateMessage` / `DispatchMessageW` loop. Callers: `0x572794` (main loop), `0x16B7F87`, `0x16B7FD2` (loading loops) |
| LookAliveCall | `0x572794` | `48 83 EC 28 E8 ? ? ? ? E8 ? ? ? ? 84 C0 74 ? E8 ? ? ? ? 80 3D @ 4` | the `E8` in function `0x572790` (no direct `E8` callers: reached through a function pointer) |
| StartupScript (new) | `0xA5ABD8` | `83 FB FF 0F 84 D6 00 00 00 @ -55` (FiveM) or `80 3D ? ? ? ? 00 48 8D 05 ? ? ? ? 4C 8D 05 ? ? ? ? 48 8D 54 24 30 48 8D 0D ? ? ? ? 4C 0F 45 C0 E8 @ -10` | loads program `"startup"` / `"startup_install"` (strings `0x1A51090` / `0x1AB1060`), creates its thread (`0x16757E4`, `0x1675C18`), calls Init `0xA58E70` and AttachScript `0x1684CE4`. Single caller `0xA58672`. FiveM hooks exactly this function (`StartupScriptWrap`) |
| ScriptIdCompare (new) | `0x11EF0` | `74 41 48 8B 01 FF 50 10 84 C0 @ -26` | script id equality (`[+8]`, `[+0x34]`, `[+0x38]`, `[+0x30]`); FiveM hooks it to return true (`ReturnTrueFromScript`) |
| CTheScripts::Shutdown (info) | `0xA61AA4` | `48 8B 01 FF 50 30 E8 ? ? ? ? E8 @ -97` (FiveM 2802+) | function start, no direct callers; FiveM resets its threads here |
| GtaThread allocation (info) | `0xA4BCD1` | `B9 60 01 00 00 E8 ? ? ? ? 48 85 C0 74 0D 48 8B C8 E8 @ 1` | `mov ecx,0x160; call alloc; call ctor 0xA49338` -> **sizeof(GtaThread) = 0x160** |

Not resolved (no pattern known; the code now works without them): GameStateChangeCall,
ViewportGame, SwapChain, ReplayInterfaces, GetEntityAddressCall, InitHUD,
CanLangChange, EventHook, BeginDisplayCall, the five native implementations
(ShutdownLoadingScreen, DoScreenFadeIn, HasScriptLoaded,
TerminateAllScriptsWithThisName, ForceCleanupForAllThreadsWithThisName) and
every `GameProcessHooks` / `UnknownPatch_4..10` patch.

Candidates that were **rejected** after reading the code around them:

* ViewportGame `33 C0 48 39 05 ? ? ? ? 74 2E 48 8B 0D ? ? ? ? 48 85 C9 74 22 @ 2`
  (unique, `0xD4EB2E`, global `0x1F6B320`): the function compares two globals
  and returns -1/0, nothing proves `0x1F6B320` is the `CViewportGame*`.
* SwapChain `4C 8B 3D ? ? ? ? 3B C8 73 0D 48 8B 05 ? ? ? ?` (unique,
  `0x13D4BAD`): indexes an array by `ecx`, does not look like the
  `IDXGISwapChain*` global. Present is hooked through a temporary swap chain
  instead.

## 3. The script thread object on 1.0.3889.0

From `ScriptThreadInit` / `Tick` / `Kill` and the allocation site, and
identical to FiveM's `GtaThreadVersion<Version, VersionAfter<Version, 2699>>`
(`code/components/rage-scripting-five/src/scrThread.cpp`):

| Offset | Field | Reference build (2017) |
|---|---|---|
| `+0x00` | vtable | same |
| `+0x08` | `scrThreadContext` (168 bytes: ThreadId, ScriptHash, State `+0x10`, IP, FrameSP, SP, TimerA/B/C, ...) | same |
| `+0xB0` | stack pointer, `+0xB8`/`+0xC0` padding, `+0xC8` exit message | same |
| `+0xD0` | `uint32 scriptHash` (added in 1.0.2699) | absent |
| `+0xD4` | `char scriptName[64]` | `+0xD0` |
| `+0x118` | `scriptHandler*` (Kill: `cmp qword [rcx+0x118],0`) | `+0x110` |
| `+0x120` | net component pointer | `+0x118` |
| `+0x140` | network id, Init sets -1 | `+0x138` |
| `+0x144` | entity handle used by Kill | `+0x13C` |
| `+0x148` | flag1 | `+0x140` |
| `+0x149` | network flag (`handler + 0x31` on both layouts) | `+0x141` |
| `+0x14E` | "paused" byte checked by Tick | `+0x146` |
| `+0x158` | canRemoveBlipsFromOtherScripts | `+0x150` |
| size | **0x160** | 0x158 |

GtaThread vtable `0x1AAF280`: `[0] 0xA49E50` destructor, `[1] 0xA5F4A0` Reset,
`[2] 0x167829C` Run (the VM), `[3] 0xA64738` Tick, `[4] 0xA5AA04` Kill,
`[5] 0xA4C5C4` CacheThreadData (added in 1.0.3407, moved to this slot in
1.0.3570; builds 3407-3569 have it at slot 0 and are not supported), then
`[6] 0xA49ECC`, `[7] 0x187F64C`.

Consequences for orange-core (implemented): the thread object is allocated
with a large zeroed tail, the script handler and network flag are reached
through the displacement read from the Kill code (`0x118` here, `0x110` on
the reference build) and a no-op `CacheThreadData` occupies vtable slot 5.

## 4. Natives

* 6701 natives are registered (walk of the obfuscated registration table in
  the dump: `natives-1.0.3889.0.extracted.txt` in the report). The
  alloc8or natives database (`natives.json`) lists exactly 6701 natives for
  this build, 29 of them new in 3889.
* No static `{hash, handler}` tables exist in `.rdata`/`.data`; registration
  is scattered code (`lea rdx,[handler]; movabs rcx,hash; jmp ...`, 680 sites
  found directly), so the registration order cannot be recovered from the
  binary.
* **FiveM publishes its universal crossmap**:
  `code/components/rage-scripting-five/include/CrossMapping_Universal.h`
  (6565 rows x 28 columns). `TableBuilder.cpp` picks the column as the number
  of entries of `{350, 372, 393, 463, 505, 573, 617, 678, 757, 791, 877, 944,
  1011, 1103, 1180, 1290, 1365, 1493, 1604, 1737, 1868, 2060, 2189, 2372,
  2545, 2802, 2944}` that are `<=` the build, so column 27 (the last) serves
  every build from 2944 on: Rockstar has not reshuffled the hashes since
  1.0.2944. Column 0 is the canonical hash (build 323); a native added later
  has zeros before the column of the build it appeared in, so its canonical
  hash is the first non-zero column.
* Checked against this dump: 6494 non-zero values in column 27, **6482 of
  them registered** on 1.0.3889.0 (12 natives removed since); 219 registered
  natives are not in the table at all: they were added after 2944, their
  canonical hash *is* their current hash, which `NativeTable` already handles
  ("no translation: use the canonical hash as it is"). `Natives.h` (5179
  canonical hashes): 5162 translate, 17 no longer exist in the game.
* FiveM's `LICENSE` puts `rage-scripting-five` outside its LGPL-covered
  directories (Take-Two / Cfx terms), so the table is **not** vendored into
  this repository; `tools/natives/crossmap_from_fivem.py` generates
  `natives-<version>.txt` from the public file on demand and the launcher can
  do the same at start.

## 5. How FiveM boots on this build (what orange-core now mirrors)

From `rage-scripting-five/src/scrEngine.cpp` (public):

1. `MH_CreateHook(StartupScript, StartupScriptWrap)`: before the game starts
   the `startup` script, create every own thread that has no thread id yet
   (`scrEngine::CreateThread`), then call the original.
2. `MH_CreateHook(GtaThread::Tick, JustNoScript)`: own threads get
   `thread->Run(0)`, every other thread returns its state untouched, i.e. the
   single player scripts never run (unless story mode is on).
3. `MH_CreateHook(ScriptIdCompare, ReturnTrueFromScript)`: entity ownership
   checks between scripts always pass.
4. `MH_CreateHook(CTheScripts::Shutdown, ...)`: own threads are reset when
   the script system shuts down.
5. `CreateThread`: free slot = first thread with `ThreadId == 0`; the thread
   gets `ThreadId = scrThreadId++`, name `scr_N`, `ScriptHash = N`,
   `scrThreadCount++`, and is stored in the collection.

## 6. Still open

* **Structures**: `GTA/CRage.h` (CPed, CVehicle, CPlayerInfo, CViewportGame),
  `GTA/CReplayInterface.h`, `GTA/VTasks.*` and the task serialisation in
  `Network/*` are 2017 layouts; the sync code will read wrong fields until
  they are updated. Public references: FiveM's headers, the `gtav-classes`
  repository, SP mod menus. Verify against the dump the same way as above.
* `ForceToSingle` / `UnknownPatch_1` / `UnknownPatch_3` match byte for byte
  but their meaning on this build is not proven; they are optional and can be
  set to `disabled` in `offsets.ini` if the game misbehaves.
* `GameStateChangeCall` and `ReplayInterfaces` were not found; neither is
  needed with the startup-script hook.
* Everything above is static analysis; the first live run on 1.0.3889.0 with
  the new code decides.
