# GTA:Orange porting evidence — 1.0.3889.0

90 offsets inventoried; 19 offset candidates; 9 required entries without a candidate.
6 resolved addresses observed in the latest matching client log (historical evidence only).
Executable: /home/sviatoslav/Projects/GTA-Orange-Reloaded/porting-report-20260908T161909870036Z/GTA5-1.0.3889.0.dump.exe.

Outputs: `offsets.candidates.ini`, `natives-1.0.3889.0.txt`, `structures.csv`, `report.json`, and copied `evidence/`.
Validated native entries extracted: 6701. Registration-site candidate groups: 0.
Live capture: stable executable bytes; unpack completion and gameplay correctness not proven.
Native mappings: 0 supplied pairs passed registration membership checks. No mapping is inferred from hash order or handler address alone.

## Still unresolved

ForceCleanupForAllThreadsWithThisName, TerminateAllScriptsWithThisName, ShutdownLoadingScreen, DoScreenFadeIn, HasScriptLoaded, CodeCave, LookAliveCall, GameStateChangeCall, ReplayInterfaces

## Interpretation

Candidates are not a completed port. A unique pattern can still target the wrong instruction.
Structure CSV contains reference comments, not discovered current offsets; task serialization and vtables still need analysis.
PE imports, when available, are in report.json; they cannot prove proxy DLL load timing.
Early loading, unpack timing and window creation were not measured. No proxy recommendation is established yet.
No game files, live configuration, process memory, anti-cheat or DRM were modified.
The app does not launch or inject into the game. Review candidates before manually installing them.
