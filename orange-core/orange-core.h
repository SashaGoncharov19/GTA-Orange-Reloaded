#pragma once
// Resolves the game offsets and applies the plain byte patches (DllMain).
bool PreLoadPatches();
// Installs the MinHook based hooks; runs on its own thread after DllMain.
void InstallGameHooks();
// MinHook helpers shared by orange-core.cpp, scrEngine.cpp and d3dhook.cpp.
bool HookAddress(void* target, void* detour, void** original, const char* what);
bool HookGameFunction(const char* offsetName, void* detour, void** original);
// Everything that used to happen at 'GameStatePlaying': script engine, D3D
// hook, input hook, the script thread. Idempotent.
void OnGameReady();
// Subclasses the game window's WndProc once the window handle is known.
void AttachInputHook();
