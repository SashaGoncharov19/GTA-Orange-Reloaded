# Updating the game offsets

`orange-core.dll` works by patching and calling code inside `GTA5.exe`. It
needs to know **where** that code is, and those addresses change with every
game update. This document explains how orange-core resolves them, what happens
on an unknown game build and how to add the offsets for a new build.

## TL;DR

1. Run the game with GTA:Orange once. On an unsupported build orange-core shows
   a message box, stays inactive and writes two things next to itself:
   `client.log` and `offsets-<game version>.generated.ini`.
2. Open the generated file: every address orange-core needs is listed with a
   description, its status (`resolved via ...` / `UNRESOLVED`) and, where one
   is known, the byte pattern that used to find it.
3. Find the addresses for your build (see [Finding offsets](#finding-offsets)),
   write them into the file, rename it to `offsets.ini` and put it next to
   `orange-core.dll`.
4. Start again. `client.log` shows every entry and where its value came from.
   Repeat until no *required* entry is unresolved.

Optional entries that you cannot find can simply be left out (or set to
`disabled`): the corresponding patch is skipped and the rest keeps working.

## How resolution works

Every address lives in one table, `g_entries` in `orange-core/GameOffsets.cpp`.
Each entry has a name, the RVA it had in the **reference build** (the game
build of January 2017 the original authors targeted), an optional byte pattern
and a `required` flag. Nothing else in the code base contains a raw address.

At startup (`GameOffsets::Initialize()`, called from `PreLoadPatches()` before
anything is touched) each entry is resolved in this order:

| Priority | Source | Notes |
|---|---|---|
| 1 | `offsets.ini`, section `[<game version>]` | exact version string of `GTA5.exe` (from its version resource, printed in `client.log`) |
| 2 | `offsets.ini`, section `[default]` | applies to every version |
| 3 | built-in reference RVA | only when the running exe **is** the reference build (five byte signatures are verified at their reference addresses first) |
| 4 | built-in byte pattern | only for entries that have one, and only if the pattern matches exactly once |
| 5 | unresolved | optional entry: skipped with a note in the log; required entry: orange-core stays inactive |

`client.log` shows the result for every entry:

```
[Info] Game version: 1.0.3411.0, image base 0x7FF6C0A20000, image size 0x4A5B000
[Info] Game build check: NOT the reference build, offsets must come from offsets.ini or pattern scans
[Info] offsets.ini loaded: 1 section(s), has a section for this game version
[Debug] offset ScrThreadCollection = 0xA1B2C3 (built-in pattern)
[Error] offset CodeCave: UNRESOLVED (required) - no pattern known for this entry
[Info] offset EscFreeze: unresolved (optional, skipped) - no pattern known for this entry
[Info] Offsets: 90 total, 0 reference, 3 from offsets.ini, 14 by pattern, 0 disabled, 73 unresolved (15 required)
```

An `orange.developer` file next to the DLL makes orange-core apply the patches
even when required entries are missing (useful for testing one hook at a
time; expect crashes) and, on the reference build, checks every built-in
pattern against the reference RVA (`pattern check <name>: OK / MISMATCH`).

## offsets.ini syntax

```ini
; comments start with ; or #
[1.0.3411.0]                       ; exact game version, see client.log
ScrThreadCollection = scan         ; use the pattern built into orange-core
CodeCave            = 0x109D5D8    ; RVA (address minus the GTA5.exe base)
LookAliveCall       = E8 ? ? ? ? 48 8B 0D ? ? ? ? 84 C0 @ 0   ; own pattern, "@ delta" optional
EscFreeze           = disabled     ; skip this optional patch

[default]                          ; every version, lower priority than [<version>]
SnowPatch = disabled
```

Names are case insensitive. Patterns are hex bytes separated by spaces, `?`
(or `??`) is a wildcard; the value is the address of the match plus the delta.
A pattern must match exactly once: an ambiguous pattern is refused, so prefer
unique patterns. Several candidates can be given separated by `|`, each with
its own `@ delta`; they are tried in order and the first unique match wins.
This mirrors how FiveM handles the same globals across game builds:

```ini
ScrThreadCollection = 48 8B C8 EB ? 33 C9 48 8B 05 @ 7 | 48 8B C8 EB 03 49 8B CD 48 8B 05 @ 8
```

The built-in table uses the same syntax for the script-engine entries; their
alternates were taken from FiveM's `rage-scripting-five` (`scrEngine.cpp`).
Mind the delta convention: FiveM points at the rel32 displacement itself,
orange-core points at the instruction (`getOffset(3)` for a 3-byte opcode
such as `48 8B 05`, `getOffset(2)` for `FF 0D` / `8B 15`), so orange-core's
delta is FiveM's minus the opcode length.

The address that has to be given is always the one orange-core **uses**:
the start of a function that is called or replaced by `ret`, the first byte
of a range that is nopped, or the instruction whose rel32 displacement points
to a global. The description of every entry in the generated template says
which one it is.

## Finding offsets

You need a disassembler (IDA, Ghidra, Binary Ninja) and a **dumped**
`GTA5.exe`: the retail executable is protected on disk, the code only exists
in clear inside the running process. The launcher writes such a dump for you:

```bash
./gta-orange-proton.sh --dump-game        # Linux / Proton
Launcher.exe --inject --dump-game         # Windows, game already running
```

It produces `GTA5-<version>.dump.exe` next to the launcher with the section
table rewritten so that file offsets equal RVAs: an address shown by the
disassembler (with the image base set to 0, or minus the image base) is the
value for `offsets.ini`. See also `docs/PORTING_STATUS.md` for the overall
picture, including the route that avoids most offsets (ScriptHookV).

Three groups of entries, from easy to hard:

### 1. Entries with a built-in pattern

The script-engine globals (`ScrThreadCollection`, `ActiveThreadTlsOffset`,
`ScrThreadId`, `ScrThreadCount`, `RegistrationTable`, `ScriptHandlerMgr`,
`GetScriptIdBlock`, `ScriptThreadTick`, `ScriptThreadKill`) carry patterns of
the kind ScriptHookV-style hooks have used for years. They are marked
*unverified* because they could not be checked against the reference build;
if they do not match on your build, search public projects that hook the same
globals (ScriptHookV-based hooks, FiveM's `rage-scripting-five`, RAGE
Multiplayer's open parts) for current patterns and put them in `offsets.ini`.

### 2. Functions that implement natives

`ShutdownLoadingScreen`, `DoScreenFadeIn`, `HasScriptLoaded`,
`ForceCleanupForAllThreadsWithThisName`, `TerminateAllScriptsWithThisName`,
`GetEntityFromScriptHandle`: these are the C++ implementations behind
well-known natives. Find the native's handler in the registration table (any
public natives database gives you the hash for your build) and follow it to
the implementation.

### 3. Patches identified only by their reference RVA

Most `GameProcessHooks` entries (`DisableNorthBlip`, `CrashLoadModelsTooQuickly`,
`RuntimeExecutableImportsCheck`, `DisablePopulation*`, ...) and the
`UnknownPatch_*` group have no pattern. The names follow the patch lists that
circulated in early GTA V multiplayer projects, so the same names (and, often,
current patterns) can be found in those projects. Work through them one by
one:

* start with everything **required**: the two `ForceToSingle` functions, the
  `CodeCave` (any 48 bytes of unused executable memory, e.g. padding at the
  end of `.text`), the four `*Call` sites, `InitHUD`, `CanLangChange`, the
  RAGE globals (`World`, `ViewportGame`, `ReplayInterfaces`, `SwapChain`,
  `GetEntityAddressCall`);
* leave optional entries unresolved at first, then add them when the base
  works.

Compare with the reference build where you can: the comments in the template
say what is patched (`5 bytes nopped`, `function start replaced by ret`,
`rel32 at +3`), and a diff of the surrounding code between builds is usually
enough to find the new location.

## What offsets do not cover

Updating the addresses is necessary but not sufficient for a modern build:

* **Native hashes.** `orange-core/Core/Natives.h` calls natives by the hash
  of the reference build (Rockstar changes the hashes every update; the
  original hash is kept in the comment of each line). A current build needs a
  translation table from those hashes to the new ones - the lookup happens in
  `ScriptEngine::GetNativeHandler()`, which is the place to add such a table.
* **Structure layouts.** `GTA/CRage.h`, `Core/scrThread.h`, the task
  serialisation and the sync code read game structures by fixed member
  offsets. Those move as well.
* **Script threads.** `Core/scrThread.cpp` mirrors the reference build's
  `scrThread` layout and virtual table.

Each of those is a separate reverse-engineering task; `client.log` and the
`orange.developer` mode are there to work through them incrementally.

## Reference

* Table and resolver: `orange-core/GameOffsets.h`, `orange-core/GameOffsets.cpp`
* INI reader, pattern scanner, value syntax: `shared/OffsetSpec.h`
  (unit tests in `tests/offsets_test.cpp`, run with `ctest`)
* Memory patching helpers: `orange-core/Memory.h` (a `CMemory` at address 0
  logs and skips every write, which is what makes unresolved optional entries
  safe)
* Template shipped with the client: `runtime/client/offsets.ini`
