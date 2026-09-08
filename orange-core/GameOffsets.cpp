// GameOffsets.cpp: the table of GTA5.exe addresses used by orange-core and
// the logic that resolves them for the game build that is actually running.
// See GameOffsets.h for the resolution order and docs/UPDATING_OFFSETS.md for
// the workflow when the game gets updated.
#include "stdafx.h"
#include "OffsetSpec.h"

using orange::BytePattern;
using orange::IniFile;
using orange::OffsetSpec;
using orange::HexString;

namespace GameOffsets
{

// ---------------------------------------------------------------------------
// The table.
//
// referenceRva is the RVA the original code used ("GetModuleHandle(NULL) +
// 0x..." plus any constant the code added before using it), so it is the
// address that is actually read or patched. Patterns marked "unverified"
// come from public ScriptHookV / FiveM style hooks for the same globals and
// have not been checked against the reference build: orange.developer mode
// logs whether they agree with the reference RVAs.
// ---------------------------------------------------------------------------
#define REQ true
#define OPT false

static const Entry g_entries[] = {
	// --- early startup / game flow (orange-core.cpp) -------------------------
	// The startup patches carry the byte patterns the original authors wrote
	// down next to them; they are applied wherever those patterns match.
	{ "StartupPatch",                     0x14493,   NULL, 0, OPT, "8 bytes at function start replaced by 'jmp short +0x90'-style skip (EB 90 90 90 90 90 90 90); early startup check, purpose undocumented" },
	{ "ForceToSingle",                    0x2773C,   "48 83 EC 28 85 D2 78 71 75 0F", 0, OPT, "function start of a game-flow dispatcher; the rel32 of the jmp at +0x3A is redirected to ForceToSingle_2 (forces the single player session). Applied only when both ForceToSingle entries resolve" },
	{ "ForceToSingle_2",                  0x186680,  "48 83 EC 28 B9 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? B1 01", 0, OPT, "function start; jump target for ForceToSingle" },
	{ "UnknownPatch_1",                   0x1B348B,  "48 85 C9 0F 84 ? 00 00 00 48 8D 55 A7 E8", 0, OPT, "the 5-byte call at +13 becomes 'mov al,1; nop nop nop' (check forced to succeed)" },
	{ "UnknownPatch_2",                   0x1AE3A0,  "E8 ? ? ? ? 8B CB 40 88 2D ? ? ? ?", 0, OPT, "5-byte call nopped" },
	{ "UnknownPatch_3",                   0x1E6EF8,  "48 89 5C 24 ? 57 48 83 EC 20 8B F9 8B DA", 0, OPT, "function start, replaced by ret" },
	{ "UnknownPatch_4",                   0x11CD8C4, NULL, 0, OPT, "function start, replaced by ret" },
	{ "UnknownPatch_5",                   0x11D03D0, NULL, 0, OPT, "function start, replaced by ret" },
	{ "UnknownPatch_6",                   0x1C220E,  NULL, 0, OPT, "9 bytes nopped" },
	{ "UnknownPatch_7",                   0xF8A528,  NULL, 0, OPT, "function start, replaced by ret" },
	{ "UnknownPatch_8",                   0x11D9868, NULL, 0, OPT, "function start, replaced by ret (original code: 0x11D986C - 4)" },
	{ "CheckMultiplayerByteDrawMapFrame", 0x23AD9C,  NULL, 0, OPT, "7 bytes replaced by 'mov sil, 1' + nops (CHECK_MULTIPLAYER_BYTE_DRAW_MAP_FRAME)" },
	{ "UnknownPatch_9",                   0x141A5,   NULL, 0, OPT, "single byte set to 0x08 (original code: 0x141A3 + 2)" },
	{ "UnknownPatch_10",                  0xA64C5C,  NULL, 0, OPT, "function start, replaced by ret (original code: 0xA64CA6 - 74)" },

	// --- game functions and globals called directly (DefineNatives) ----------
	// All optional: where a function is unresolved the native behind it is
	// used, or (script shutdown) the stock scripts are simply never ticked.
	{ "ForceCleanupForAllThreadsWithThisName", 0xC70970, NULL, 0, OPT, "void(const char* scriptName, int mask); used to stop the single player scripts by name (reference build route)" },
	{ "TerminateAllScriptsWithThisName",  0xA3DAE8,  NULL, 0, OPT, "void(const char* scriptName); used to stop the single player scripts by name (reference build route)" },
	{ "ShutdownLoadingScreen",            0x1FBD34,  NULL, 0, OPT, "void(); native SHUTDOWN_LOADING_SCREEN implementation; the native is called from the script thread when unresolved" },
	{ "DoScreenFadeIn",                   0x2A1554,  NULL, 0, OPT, "void(int64 duration); native DO_SCREEN_FADE_IN implementation; the native is called from the script thread when unresolved" },
	{ "HasScriptLoaded",                  0xCE37E0,  NULL, 0, OPT, "bool(const char* scriptName); native HAS_SCRIPT_LOADED implementation (reference build route)" },
	{ "CanLangChange",                    0x1C183F,  NULL, 0, OPT, "instruction with a 2-byte opcode + rel32 to a global; global + 1 is the 'language can change' bool toggled while the chat is open (a private bool is used when unresolved)" },
	{ "InitializeOnline",                 0x103708,  NULL, 0, OPT, "void(); not called by the current code" },
	{ "InitHUD",                          0x1F356C,  NULL, 0, OPT, "void(); HUD initialisation called once from the per-frame hook, skipped when unresolved (original code: 0x1F358F - 0x23)" },
	{ "EventHook",                        0x7FFF0C,  NULL, 0, OPT, "task event function start, replaced by a far jump to an empty handler" },

	// --- main loop hooks (GameHooks.cpp, orange-core.cpp) ---------------------
	// orange-core needs one per-frame hook (LookAlive or LookAliveCall) and one
	// 'game ready' trigger (StartupScript, GameStateChangeCall or the World
	// ped poll); PreLoadPatches() checks that.
	{ "CodeCave",                         0x109D5D8, NULL, 0, OPT, "48 bytes of unused executable memory for the far jumps of the call-site hooks; when unresolved a page within 2 GB of GTA5.exe is allocated instead" },
	{ "LookAlive",                        0,         "40 55 53 56 57 41 57 48 8D 6C 24 C9 48 81 EC 90 00 00 00 8B 05", 0, OPT, "function start of the per-frame window message pump (SetThreadExecutionState + PeekMessage loop) the main loop calls every frame; hooked with MinHook. Preferred over LookAliveCall" },
	{ "LookAliveCall",                    0x67AE,    "48 83 EC 28 E8 ? ? ? ? E8 ? ? ? ? 84 C0 74 ? E8 ? ? ? ? 80 3D", 4, OPT, "E8 call to the per-frame 'LookAlive' function; redirected through the code cave when LookAlive is unresolved (original code: 0x67A7 + 7)" },
	{ "StartupScript",                    0,         "83 FB FF 0F 84 D6 00 00 00 @ -55 | 80 3D ? ? ? ? 00 48 8D 05 ? ? ? ? 4C 8D 05 ? ? ? ? 48 8D 54 24 30 48 8D 0D ? ? ? ? 4C 0F 45 C0 E8 @ -10", 0, OPT, "function start of 'start the startup script' (loads the 'startup' program and creates its thread); hooked with MinHook, GTA:Orange initialises its script engine and creates its own thread right before it, the way FiveM does. Preferred 'game ready' trigger" },
	{ "GameStateChangeCall",              0x1EC8FA,  NULL, 0, OPT, "E8 call to the game state change function; redirected through the code cave. 'Game ready' trigger of the reference build" },
	{ "WindowCreateCall",                 0x12416F7, "4C 8B C1 8B CE FF 15 ? ? ? ? 41 8B D4 48 8B C8 48 8B D8 FF 15", 5, OPT, "6-byte FF 15 call to CreateWindowExW creating the game window, replaced by a 5-byte call through the code cave + nop (window title and icon); the window handle is taken from the swap chain when unresolved" },
	{ "BeginDisplayCall",                 0xCF31CB,  NULL, 0, OPT, "E8 call to DrawTextManager::BeginDisplay; redirected through the code cave" },

	// --- gameplay patches (GameProcessHooks), all optional --------------------
	{ "ObjectsPatch",                     0xC91D39,  NULL, 0, OPT, "24 bytes nopped (object handling)" },
	{ "EscFreeze",                        0x1F5BEC,  NULL, 0, OPT, "5 bytes nopped: the game no longer freezes when the pause menu (Esc) opens" },
	{ "CheatConsole",                     0x7B2F6C,  NULL, 0, OPT, "function start replaced by ret: cheat console disabled" },
	{ "UIWheelSlowmo",                    0x2413D2,  NULL, 0, OPT, "6 bytes nopped: no slow motion while the weapon wheel is open" },
	{ "ShowCursor_1",                     0x127C8CA, NULL, 0, OPT, "4 bytes nopped (ShowCursor handling)" },
	{ "ShowCursor_2",                     0x127C8DC, NULL, 0, OPT, "4 bytes nopped (ShowCursor handling)" },
	{ "RockstarLoadingLogo",              0x1BCCE8,  NULL, 0, OPT, "function start replaced by ret: no Rockstar loading logo" },
	{ "Tooltips",                         0x1C8E28,  NULL, 0, OPT, "function start replaced by ret: loading tooltips disabled" },
	{ "SocialClubNews",                   0x10046F0, NULL, 0, OPT, "function start replaced by ret: Social Club news disabled" },
	{ "DisableWantedGeneration_1",        0x62039C,  NULL, 0, OPT, "function start replaced by ret" },
	{ "DisableWantedGeneration_2",        0x6194BE,  NULL, 0, OPT, "2 bytes replaced by 'jmp short' (90 E9 written as 0xE990 little endian)" },
	{ "IntentionalCrash",                 0x11D2BD3, NULL, 0, OPT, "5 bytes nopped: intentional anti-tamper crash call" },
	{ "CrashLoadModelsTooQuickly",        0xCCC992,  NULL, 0, OPT, "23 bytes nopped (CRASH_LOAD_MODELS_TOO_QUICKLY)" },
	{ "CreateNetworkEventBindings",       0x109D5D4, NULL, 0, OPT, "function start replaced by ret (CREATE_NETWORK_EVENT_BINDINGS)" },
	{ "LoadNewGame",                      0x597484,  NULL, 0, OPT, "function start replaced by ret (LOAD_NEW_GAME)" },
	{ "ResetVehicleDensityLastFrame",     0xEE29C8,  NULL, 0, OPT, "function start replaced by ret (RESET_VEHICLE_DENSITY_LAST_FRAME)" },
	{ "VarVehicleDensity",                0xEE29D7,  NULL, 0, OPT, "instruction with a 2-byte opcode + rel32 to the vehicle density global, which is set to 0 (VAR_VEHICLE_DENSITY)" },
	{ "SetClockForwardAfterDeath",        0x5BB16C,  NULL, 0, OPT, "function start replaced by ret (SET_CLOCK_FORWARD_AFTER_DEATH)" },
	{ "DisableNorthBlip",                 0x1788CC,  NULL, 0, OPT, "46 bytes nopped (DISABLE_NORTH_BLIP)" },
	{ "DisableVehicleResetAtSetPosition", 0xEE22B8,  NULL, 0, OPT, "function start replaced by ret (DISABLE_VEHICLE_RESET_AT_SET_POSITION)" },
	{ "DisableLoadingMpDlcContent",       0x8F6624,  NULL, 0, OPT, "function start replaced by ret (DISABLE_LOADING_MP_DLC_CONTENT)" },
	{ "RuntimeExecutableImportsCheck",    0x9F69D4,  NULL, 0, OPT, "function start replaced by ret (RUNTIME_EXECUTABLE_IMPORTS_CHECK)" },
	{ "DisablePopulationVehicles_10",     0xE66118,  NULL, 0, OPT, "function start replaced by ret" },
	{ "DisablePopulationVehicles_8",      0xEF9670,  NULL, 0, OPT, "function start replaced by ret" },
	{ "DisablePopulationVehicles_11a",    0xE9BE73,  NULL, 0, OPT, "3 bytes nopped" },
	{ "DisablePopulationVehicles_11b",    0xE3CC85,  NULL, 0, OPT, "5 bytes nopped" },
	{ "DisablePopulationPeds_1",          0x442F4C,  NULL, 0, OPT, "function start replaced by ret" },
	{ "DisablePopulationPeds_2",          0x694030,  NULL, 0, OPT, "function start replaced by ret" },
	{ "DisablePopulationAmbientPeds",     0x6AC43D,  NULL, 0, OPT, "the 32-bit immediates at +6 and +16 are set to 0" },
	{ "DisablePopulationPeds_4",          0x6BF525,  NULL, 0, OPT, "20 bytes nopped" },
	{ "DisableCopsAndFireTrucks_1",       0x5FA314,  NULL, 0, OPT, "function start replaced by ret" },
	{ "DisableCopsAndFireTrucks_2",       0x33E78C,  NULL, 0, OPT, "function start replaced by ret" },
	{ "DisableCopsAndFireTrucks_3",       0x61F620,  NULL, 0, OPT, "function start replaced by ret" },
	{ "SnowPatch",                        0x4E1FA4,  NULL, 0, OPT, "20 bytes nopped by the /snow debug command" },

	// --- script engine (Core/scrEngine.cpp, Core/scrThread.cpp) ---------------
	// Patterns come from FiveM's rage-scripting-five (scrEngine.cpp,
	// scrThread.cpp), which keeps one variant per family of game builds, and
	// were verified on 1.0.3889.0. FiveM points at the rel32 itself; our code
	// points at the instruction (getOffset(3) / getOffset(2)), hence the
	// deltas differ from theirs.
	{ "ScrThreadCollection",              0x9DF347,
	  "48 8B C8 EB ? 33 C9 48 8B 05 @ 7 | 48 8B C8 EB 03 49 8B CD 48 8B 05 @ 8 | 48 8B C8 EB 03 48 8B CB 48 8B 05 @ 8", 8, REQ,
	  "'mov rax, [rip+X]' loading the script thread collection (atArray of GtaThread*); rel32 at +3 (original code: 0x9DF33F + 8)" },
	{ "ActiveThreadTlsOffset",            0x14AE8E9,
	  "48 8B 04 D0 4A 8B 14 00 48 8B 01 F3 44 0F 2C 42 20 @ -4 | 48 8B 04 D0 4A 8B 14 00 48 8B 01 F3 0F 10 40 20 @ -4", -4, REQ,
	  "32-bit immediate: TLS offset of the active script thread (original code: 0x14AE8ED - 4)" },
	{ "ScrThreadId",                      0x30A9E0B,
	  "8B 15 ? ? ? ? 48 8B 05 ? ? ? ? FF C2 89 15 ? ? ? ? 48 8B 0C F8 @ 0 | 8B 15 ? ? ? ? 48 8B 05 ? ? ? ? FF C2 89 15 ? ? ? ? 48 8B 0C D8 @ 0 | 8B 15 ? ? ? ? 48 8B 05 ? ? ? ? FF C2 89 15 ? ? ? ? E9 @ 0 | 8B 15 ? ? ? ? 48 8B 05 ? ? ? ? FF C2 89 @ 0 | 89 15 ? ? ? ? 48 8B 0C D8 @ 0", 0, REQ,
	  "'mov edx, [rip+X]' / 'mov [rip+X], edx' of the next script thread id; rel32 at +2 (original code: 0x30A9E04 + 7)" },
	{ "ScrThreadCount",                   0x14AFE13,
	  "FF 0D ? ? ? ? 48 8B D9 75 @ 0 | FF 0D ? ? ? ? 48 8B F9 @ 0", 0, REQ,
	  "'dec dword [rip+X]' of the script thread count; rel32 at +2" },
	{ "RegistrationTable",                0x14B1A55, "76 32 48 8B 53 40 @ 6 | 76 61 49 8B 7A 40 48 8D 0D @ 6", 6, REQ, "'lea rcx, [rip+X]' of the native registration table (256 buckets); rel32 at +3 (original code: 0x14B1A4F + 6)" },
	{ "ScriptHandlerMgr",                 0x9ED224,  "74 17 48 8B C8 E8 ? ? ? ? 48 8D 0D", 10, REQ, "'lea rcx, [rip+X]' of the script handler manager (AttachScript = vtable slot 10); rel32 at +3 (original code: 0x9ED21A + 10)" },
	{ "ScriptIdCompare",                  0,         "74 41 48 8B 01 FF 50 10 84 C0 @ -26 | 74 3C 48 8B 01 FF 50 10 84 C0 @ -26", 0, OPT, "function start of the script id comparison used by 'may this script use this entity' checks; hooked with MinHook to answer yes while the stock scripts are disabled (as FiveM does)" },
	{ "ScriptThreadTick",                 0x9F645C,  "80 B9 46 01 00 00 00 8B FA 48 8B D9 74 05 @ -15 | 80 B9 ? 01 00 00 00 8B FA 48 8B D9 74 05 @ -15", -0xF, REQ, "eThreadState __thiscall(scrThread*, uint32 opsToExecute) = GtaThread::Tick; also hooked with MinHook so that only GTA:Orange's threads run and the stock single player scripts stay frozen (original code: 0x9F646B - 0xF)" },
	{ "ScriptThreadKill",                 0x9ECF6C,  "48 83 EC 20 48 83 B9 ? 01 00 00 00 48 8B D9 74 14", -6, REQ, "void __thiscall(scrThread*) = GtaThread::Kill; the displacement of its 'cmp qword [rcx+X], 0' at +0xA tells where the script handler lives in the thread object (original code: 0x9ECF72 - 6)" },
	{ "ScriptThreadInit",                 0x9EB4DC,  "83 89 ? 01 00 00 FF 83 A1 ? 01 00 00 F0", 0, REQ, "void __thiscall(scrThread*); initialises the GTA part of a freshly reset script thread" },

	// --- RAGE globals (GTA/CRage.cpp, GTA/CReplayInterface.cpp) ---------------
	{ "PlayerColor",                      0x1E5C90,  NULL, 0, OPT, "instruction with a 2-byte opcode + rel32 to the player colour table; 4 BGRA entries start at +4" },
	{ "ViewportGame",                     0xA27578,  NULL, 0, OPT, "instruction with a 3-byte opcode + rel32 to the CViewportGame* global (view matrix at +0x24C, size at +0x450 on the reference build); natives do the world-to-screen projection when unresolved" },
	{ "GetEntityFromScriptHandle",        0x15013C,  "83 F9 FF 74 ? 8B D1 C1 FA 08 85 D2 78 ? 4C 8B 05 ? ? ? ? 41 3B 50 10", 0, OPT, "CEntity*(int scriptHandle) = fwScriptGuid::GetBaseFromGuid; resolves script handles to game entities" },
	{ "World",                            0x89E04D,  "48 8B 05 ? ? ? ? 48 8B 40 08 C3", 0, OPT, "instruction with a 3-byte opcode + rel32 to the CWorld* (ped factory) global whose +8 is the local player ped; also the fallback 'game ready' poll" },
	{ "VehicleFactory",                   0xE43BF4,  NULL, 0, OPT, "instruction with a 3-byte opcode + rel32 to the vehicle factory; not used by the current code" },
	{ "ReplayInterfaces",                 0x1CB4,    NULL, 0, OPT, "instruction with a 3-byte opcode + rel32 to the ReplayInterfaces* global (entity pools); only the debug overlay reads it" },
	{ "GetEntityAddressCall",             0xA29ECE,  NULL, 0, OPT, "7-byte instruction whose rel32 at +3 points to the 'entity address from handle' function; GetEntityFromScriptHandle is used instead when unresolved" },

	// --- allocator and task sync (GTA/sysAllocator.cpp, Network/*) -----------
	{ "HeapTask",                         0x4E05,    NULL, 0, OPT, "instruction with a 3-byte opcode + rel32 to the task heap allocator global" },
	{ "HeapTaskClone",                    0x6369FA,  NULL, 0, OPT, "instruction with a 3-byte opcode + rel32 to the task clone heap allocator global" },
	{ "SysAllocate",                      0x11D7D6C, NULL, 0, OPT, "void*(int64 size, int64 align, int64 heap, int64 suballocator)" },
	{ "SysFree",                          0x11D7DDC, NULL, 0, OPT, "void(int64 heap, void* memory)" },
	{ "CreateTaskInfoById",               0x658904,  NULL, 0, OPT, "CSerialisedFSMTaskInfo*(unsigned int taskId); task synchronisation" },
	{ "RageBufferInit",                   0x11E7920, NULL, 0, OPT, "void(rageBuffer*, unsigned char* data, int bits, int flags); initialises a write buffer" },
	{ "RageBufferInitRead",               0x11EBCA8, NULL, 0, OPT, "void(rageBuffer*, unsigned char* data, int bits, int flags); initialises a read buffer" },
	{ "ShowAbilityBar",                   0x1F26D4,  NULL, 0, OPT, "int(bool show); hides the special ability bar" },

	// --- rendering ------------------------------------------------------------
	{ "SwapChain",                        0x124BDC5, NULL, 0, OPT, "instruction with a 3-byte opcode + rel32 to the game's IDXGISwapChain* global; when unresolved Present is hooked through the vtable of a temporary swap chain, which works on every build" },
	{ "ScaleformManager",                 0x1F3A868, NULL, 0, OPT, "rage::ScaleformManager* global (direct pointer, not rel32); only used with ORANGE_ENABLE_SCALEFORM" },
	{ "ScaleformCreateText",              0x15ECB18, NULL, 0, OPT, "GFx DrawTextManager::CreateText; only used with ORANGE_ENABLE_SCALEFORM" },
};
#undef REQ
#undef OPT

static const size_t g_entryCount = sizeof(g_entries) / sizeof(g_entries[0]);

// Byte patches inherited from the 2017 patch lists: they change the game's
// code, and what they mean is only established for the reference build. On any
// other build a pattern match proves that the bytes look alike, not that the
// instruction plays the same role there, and a wrong patch in the boot path
// stops the game from starting at all ("failed to initialize"). They are
// therefore skipped on a non-reference build unless offsets.ini asks for one by
// name ("ForceToSingle = scan", or an RVA). Reads and hooks orange-core needs
// to work at all are not in this list.
static const char* const g_unverifiedPatches[] = {
	"StartupPatch", "ForceToSingle", "ForceToSingle_2",
	"UnknownPatch_1", "UnknownPatch_2", "UnknownPatch_3", "UnknownPatch_4", "UnknownPatch_5",
	"UnknownPatch_6", "UnknownPatch_7", "UnknownPatch_8", "UnknownPatch_9", "UnknownPatch_10",
	"CheckMultiplayerByteDrawMapFrame", "EventHook",
	"ObjectsPatch", "EscFreeze", "CheatConsole", "UIWheelSlowmo", "ShowCursor_1", "ShowCursor_2",
	"RockstarLoadingLogo", "Tooltips", "SocialClubNews",
	"DisableWantedGeneration_1", "DisableWantedGeneration_2", "IntentionalCrash",
	"CrashLoadModelsTooQuickly", "CreateNetworkEventBindings", "LoadNewGame",
	"ResetVehicleDensityLastFrame", "VarVehicleDensity", "SetClockForwardAfterDeath",
	"DisableNorthBlip", "DisableVehicleResetAtSetPosition", "DisableLoadingMpDlcContent",
	"RuntimeExecutableImportsCheck",
	"DisablePopulationVehicles_10", "DisablePopulationVehicles_8",
	"DisablePopulationVehicles_11a", "DisablePopulationVehicles_11b",
	"DisablePopulationPeds_1", "DisablePopulationPeds_2", "DisablePopulationAmbientPeds",
	"DisablePopulationPeds_4",
	"DisableCopsAndFireTrucks_1", "DisableCopsAndFireTrucks_2", "DisableCopsAndFireTrucks_3",
	"SnowPatch",
};

static bool IsUnverifiedPatch(const char* name)
{
	for (const char* patch : g_unverifiedPatches)
		if (_stricmp(patch, name) == 0)
			return true;
	return false;
}

// Entries whose pattern was written down by the original authors next to the
// RVA; all five matching at their reference RVA identifies the reference build.
static const char* const g_referenceSignatures[] = {
	"ForceToSingle", "ForceToSingle_2", "UnknownPatch_1", "UnknownPatch_2", "UnknownPatch_3"
};

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static bool g_initialized = false;
static bool g_referenceBuild = false;
static uintptr_t g_base = 0;
static size_t g_imageSize = 0;
static std::string g_gameVersion = "unknown";
static std::string g_gameExe;
static std::vector<Status> g_status;
static std::map<std::string, size_t> g_index;   // lower-case name -> index into g_status

const char* SourceName(Source source)
{
	switch (source)
	{
	case Source::Disabled:  return "disabled";
	case Source::Reference: return "reference build";
	case Source::Ini:       return "offsets.ini";
	case Source::Scan:      return "pattern scan";
	default:                return "unresolved";
	}
}

// ---------------------------------------------------------------------------
// Memory access guarded by SEH. These helpers must not contain objects with
// destructors (MSVC C2712), which is why they only take pointers.
// ---------------------------------------------------------------------------
static size_t SafeFind(const BytePattern* pattern, const uint8_t* data, size_t size, size_t start)
{
	__try
	{
		return pattern->Find(data, size, start);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return BytePattern::npos;
	}
}

static bool SafeMatch(const BytePattern* pattern, const uint8_t* data)
{
	__try
	{
		return pattern->Matches(data);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

struct Region
{
	uintptr_t start;
	size_t size;
};

// Committed, readable parts of the main module (skips guard / no-access pages).
static std::vector<Region> ReadableRegions()
{
	std::vector<Region> regions;
	uintptr_t address = g_base;
	const uintptr_t end = g_base + g_imageSize;
	MEMORY_BASIC_INFORMATION mbi;
	while (address < end && VirtualQuery((LPCVOID)address, &mbi, sizeof(mbi)) == sizeof(mbi))
	{
		uintptr_t regionStart = (uintptr_t)mbi.BaseAddress;
		uintptr_t regionEnd = regionStart + mbi.RegionSize;
		bool readable = mbi.State == MEM_COMMIT
			&& mbi.Protect != 0
			&& !(mbi.Protect & PAGE_NOACCESS)
			&& !(mbi.Protect & PAGE_GUARD);
		if (readable)
		{
			uintptr_t s = regionStart < g_base ? g_base : regionStart;
			uintptr_t e = regionEnd > end ? end : regionEnd;
			if (e > s)
				regions.push_back({ s, (size_t)(e - s) });
		}
		if (regionEnd <= address)
			break;
		address = regionEnd;
	}
	return regions;
}

// Scans the whole module. Returns the number of matches found (stops at 2)
// and the RVA of the first one.
static size_t ScanPattern(const BytePattern& pattern, int delta, uintptr_t& rvaOut)
{
	size_t matches = 0;
	rvaOut = 0;
	std::vector<Region> regions = ReadableRegions();
	for (size_t r = 0; r < regions.size() && matches < 2; ++r)
	{
		size_t pos = 0;
		while (matches < 2)
		{
			pos = SafeFind(&pattern, (const uint8_t*)regions[r].start, regions[r].size, pos);
			if (pos == BytePattern::npos)
				break;
			if (matches == 0)
				rvaOut = (uintptr_t)((intptr_t)(regions[r].start + pos - g_base) + delta);
			++matches;
			++pos;
		}
	}
	return matches;
}

static bool MatchesAt(const BytePattern& pattern, uintptr_t rva)
{
	if (rva + pattern.Length() > g_imageSize)
		return false;
	return SafeMatch(&pattern, (const uint8_t*)(g_base + rva));
}

// Tries the candidates in order; the first one that matches exactly once
// wins. `note` explains the outcome either way.
static bool ScanCandidates(const std::vector<orange::PatternCandidate>& candidates, uintptr_t& rvaOut, std::string& note)
{
	size_t notFound = 0, ambiguous = 0;
	for (size_t i = 0; i < candidates.size(); ++i)
	{
		uintptr_t rva = 0;
		size_t matches = ScanPattern(candidates[i].pattern, candidates[i].delta, rva);
		if (matches == 1)
		{
			rvaOut = rva;
			note = candidates.size() > 1
				? "pattern #" + std::to_string(i + 1) + " of " + std::to_string(candidates.size())
				: std::string("pattern");
			return true;
		}
		if (matches == 0)
			++notFound;
		else
			++ambiguous;
	}
	if (candidates.empty())
		note = "no pattern";
	else if (ambiguous)
		note = std::to_string(ambiguous) + " pattern(s) ambiguous, " + std::to_string(notFound) + " not found, refusing to guess";
	else
		note = std::to_string(notFound) + " pattern(s) not found";
	return false;
}

static std::string ReadFileVersion(const std::string& path)
{
	DWORD handle = 0;
	DWORD size = GetFileVersionInfoSizeA(path.c_str(), &handle);
	if (!size)
		return "unknown";
	std::vector<char> buffer(size);
	if (!GetFileVersionInfoA(path.c_str(), 0, size, buffer.data()))
		return "unknown";
	VS_FIXEDFILEINFO* info = NULL;
	UINT length = 0;
	if (!VerQueryValueA(buffer.data(), "\\", (LPVOID*)&info, &length) || !info || length < sizeof(VS_FIXEDFILEINFO))
		return "unknown";
	char text[64];
	snprintf(text, sizeof(text), "%u.%u.%u.%u",
		HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
		HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
	return text;
}

static const Entry* FindEntry(const char* name)
{
	for (size_t i = 0; i < g_entryCount; ++i)
		if (_stricmp(g_entries[i].name, name) == 0)
			return &g_entries[i];
	return NULL;
}

static bool DetectReferenceBuild()
{
	bool all = true;
	for (const char* name : g_referenceSignatures)
	{
		const Entry* entry = FindEntry(name);
		std::vector<orange::PatternCandidate> candidates;
		if (!entry || !entry->pattern || !orange::ParsePatternCandidates(entry->pattern, entry->patternDelta, candidates))
		{
			all = false;
			continue;
		}
		if (!MatchesAt(candidates[0].pattern, entry->referenceRva - candidates[0].delta))
		{
			log_info << "Reference signature " << name << " does not match at GTA5.exe+" << HexString(entry->referenceRva) << std::endl;
			all = false;
		}
	}
	return all;
}

static bool ReadTextFile(const std::string& path, std::string& out)
{
	std::ifstream in(path, std::ios::binary);
	if (!in.good())
		return false;
	std::stringstream ss;
	ss << in.rdbuf();
	out = ss.str();
	return true;
}

// Applies one offsets.ini value to a status entry. Returns true when the
// value decided the entry (resolved, disabled or a failed explicit request).
static bool ApplyIniValue(Status& status, const std::string& value, const std::string& section)
{
	OffsetSpec spec;
	std::string error;
	if (!spec.Parse(value, &error))
	{
		log_error << "offsets.ini [" << section << "] " << status.entry->name << ": " << error << std::endl;
		status.note = "offsets.ini: " + error;
		return false;
	}

	switch (spec.kind)
	{
	case OffsetSpec::Disabled:
		status.source = Source::Disabled;
		status.rva = 0;
		status.note = "disabled in offsets.ini [" + section + "]";
		return true;

	case OffsetSpec::Rva:
		if (spec.rva == 0 || spec.rva >= g_imageSize)
		{
			log_error << "offsets.ini [" << section << "] " << status.entry->name << ": " << HexString(spec.rva)
				<< " is outside GTA5.exe (image size " << HexString(g_imageSize) << ")" << std::endl;
			status.note = "offsets.ini value outside the image";
			return true;
		}
		status.source = Source::Ini;
		status.rva = (uintptr_t)spec.rva;
		status.note = "offsets.ini [" + section + "]";
		return true;

	case OffsetSpec::Scan:
	case OffsetSpec::Pattern:
	{
		std::vector<orange::PatternCandidate> candidates;
		if (spec.kind == OffsetSpec::Pattern)
			candidates = spec.candidates;
		else if (!status.entry->pattern || !orange::ParsePatternCandidates(status.entry->pattern, status.entry->patternDelta, candidates))
		{
			log_error << "offsets.ini [" << section << "] " << status.entry->name << ": 'scan' requested but orange-core has no usable pattern for it" << std::endl;
			status.note = "no built-in pattern to scan for";
			return true;
		}

		uintptr_t rva = 0;
		std::string note;
		if (!ScanCandidates(candidates, rva, note))
		{
			log_error << "offsets.ini [" << section << "] " << status.entry->name << ": " << note << std::endl;
			status.note = "offsets.ini: " + note;
			return true;
		}
		status.source = Source::Scan;
		status.rva = rva;
		status.note = std::string("offsets.ini [") + section + "] " + note;
		return true;
	}
	}
	return false;
}

static void ResolveWithBuiltins(Status& status)
{
	const Entry& entry = *status.entry;

	if (g_referenceBuild && entry.referenceRva != 0)
	{
		status.source = Source::Reference;
		status.rva = entry.referenceRva;
		status.note = "reference build";
		return;
	}

	if (entry.pattern)
	{
		std::vector<orange::PatternCandidate> candidates;
		std::string error;
		if (!orange::ParsePatternCandidates(entry.pattern, entry.patternDelta, candidates, &error))
		{
			status.note = "built-in pattern is malformed: " + error;
			return;
		}
		uintptr_t rva = 0;
		std::string note;
		if (ScanCandidates(candidates, rva, note))
		{
			status.source = Source::Scan;
			status.rva = rva;
			status.note = "built-in " + note;
			return;
		}
		status.note = "built-in " + note;
		return;
	}

	status.note = g_referenceBuild ? "no reference RVA" : "no pattern known for this entry";
}

// orange.developer on the reference build: report whether the unverified
// built-in patterns land on the reference RVAs.
static void VerifyPatternsAgainstReference()
{
	for (size_t i = 0; i < g_entryCount; ++i)
	{
		const Entry& entry = g_entries[i];
		if (!entry.pattern || !entry.referenceRva)
			continue;
		std::vector<orange::PatternCandidate> candidates;
		if (!orange::ParsePatternCandidates(entry.pattern, entry.patternDelta, candidates))
			continue;
		uintptr_t rva = 0;
		std::string note;
		if (ScanCandidates(candidates, rva, note) && rva == entry.referenceRva)
			log_info << "pattern check " << entry.name << ": OK (" << note << ")" << std::endl;
		else if (rva)
			log_error << "pattern check " << entry.name << ": MISMATCH, " << note << " found " << HexString(rva)
				<< ", reference " << HexString(entry.referenceRva) << std::endl;
		else
			log_error << "pattern check " << entry.name << ": " << note << std::endl;
	}
}

bool Initialize()
{
	if (g_initialized)
		return AllRequiredResolved();
	g_initialized = true;

	g_base = (uintptr_t)GetModuleHandle(NULL);
	MODULEINFO info = { 0 };
	GetModuleInformation(GetCurrentProcess(), GetModuleHandle(NULL), &info, sizeof(info));
	g_imageSize = info.SizeOfImage;

	char exePath[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, exePath, MAX_PATH);
	g_gameExe = exePath;
	g_gameVersion = ReadFileVersion(g_gameExe);
	log_info << "Game executable: " << g_gameExe << std::endl;
	log_info << "Game version: " << g_gameVersion << ", image base " << HexString(g_base) << ", image size " << HexString(g_imageSize) << std::endl;

	g_status.clear();
	g_index.clear();
	for (size_t i = 0; i < g_entryCount; ++i)
	{
		Status s;
		s.entry = &g_entries[i];
		s.source = Source::Unresolved;
		s.rva = 0;
		std::string key = orange::LowerCopy(g_entries[i].name);
		if (g_index.count(key))
			log_error << "GameOffsets: duplicate entry name " << g_entries[i].name << std::endl;
		g_index[key] = g_status.size();
		g_status.push_back(s);
	}

	g_referenceBuild = DetectReferenceBuild();
	log_info << (g_referenceBuild
		? "Game build check: this is the reference build, built-in offsets apply"
		: "Game build check: NOT the reference build, offsets must come from offsets.ini or pattern scans") << std::endl;

	// offsets.ini
	IniFile ini;
	bool haveIni = false;
	std::string iniPath = CGlobals::Get().orangePath + "\\offsets.ini";
	std::string iniText;
	if (ReadTextFile(iniPath, iniText))
	{
		haveIni = true;
		ini.Parse(iniText);
		for (const std::string& err : ini.errors)
			log_error << "offsets.ini " << err << std::endl;
		log_info << "offsets.ini loaded: " << ini.sections.size() << " section(s)"
			<< (ini.HasSection(g_gameVersion) ? ", has a section for this game version" : ", no section for this game version")
			<< (ini.HasSection("default") ? ", has [default]" : "") << std::endl;
	}
	else
		log_info << "offsets.ini not found (" << iniPath << "), using built-in offsets only" << std::endl;

	size_t counts[5] = { 0, 0, 0, 0, 0 };
	size_t unresolvedRequired = 0;
	size_t unverifiedSkipped = 0;
	for (Status& status : g_status)
	{
		bool decided = false;
		if (haveIni)
		{
			const std::string* value = ini.Get(g_gameVersion, status.entry->name);
			std::string section = g_gameVersion;
			if (!value)
			{
				value = ini.Get("default", status.entry->name);
				section = "default";
			}
			if (value)
				decided = ApplyIniValue(status, *value, section);
		}
		if (!decided)
		{
			// A code patch nobody has confirmed for this build is more likely
			// to break the game than to help; offsets.ini opts back in.
			if (!g_referenceBuild && IsUnverifiedPatch(status.entry->name))
			{
				status.source = Source::Unresolved;
				status.rva = 0;
				status.note = "unverified patch, not applied on a build other than the reference one "
					"(put \"" + std::string(status.entry->name) + " = scan\" in offsets.ini to apply it anyway)";
				++unverifiedSkipped;
			}
			else
				ResolveWithBuiltins(status);
		}

		if (status.rva != 0 && status.rva >= g_imageSize)
		{
			log_error << "offset " << status.entry->name << ": " << HexString(status.rva) << " is outside the image, ignored" << std::endl;
			status.source = Source::Unresolved;
			status.rva = 0;
			status.note = "outside the image";
		}

		counts[(int)status.source]++;
		if (status.source == Source::Unresolved)
		{
			if (status.entry->required)
			{
				++unresolvedRequired;
				log_error << "offset " << status.entry->name << ": UNRESOLVED (required) - " << status.note << std::endl;
			}
			else
				log_info << "offset " << status.entry->name << ": unresolved (optional, skipped) - " << status.note << std::endl;
		}
		else
			log_debug << "offset " << status.entry->name << " = " << HexString(status.rva) << " (" << status.note << ")" << std::endl;
	}

	log_info << "Offsets: " << g_status.size() << " total, "
		<< counts[(int)Source::Reference] << " reference, "
		<< counts[(int)Source::Ini] << " from offsets.ini, "
		<< counts[(int)Source::Scan] << " by pattern, "
		<< counts[(int)Source::Disabled] << " disabled, "
		<< counts[(int)Source::Unresolved] << " unresolved (" << unresolvedRequired << " required)" << std::endl;
	if (unverifiedSkipped)
		log_info << "Offsets: " << unverifiedSkipped << " unverified code patch(es) were NOT applied on this build; "
			"orange-core only reads the game and installs its hooks. See docs/UPDATING_OFFSETS.md to enable them one by one." << std::endl;

	if (g_referenceBuild && CGlobals::Get().isDeveloper)
		VerifyPatternsAgainstReference();

	if (!g_referenceBuild || unresolvedRequired || CGlobals::Get().isDeveloper)
	{
		std::string templatePath = CGlobals::Get().orangePath + "\\offsets-" + g_gameVersion + ".generated.ini";
		if (WriteTemplate(templatePath))
			log_info << "Offsets template written to " << templatePath << std::endl;
	}

	return unresolvedRequired == 0;
}

bool IsInitialized() { return g_initialized; }

int GameBuildNumber()
{
	unsigned a = 0, b = 0, c = 0, d = 0;
	if (sscanf_s(g_gameVersion.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) < 3)
		return 0;
	return (int)c;
}
bool IsReferenceBuild() { return g_referenceBuild; }
const std::string& GameVersion() { return g_gameVersion; }
const std::vector<Status>& All() { return g_status; }

bool AllRequiredResolved()
{
	if (!g_initialized)
		return false;
	for (const Status& s : g_status)
		if (s.entry->required && s.source == Source::Unresolved)
			return false;
	return true;
}

std::vector<std::string> UnresolvedRequired()
{
	std::vector<std::string> names;
	for (const Status& s : g_status)
		if (s.entry->required && s.source == Source::Unresolved)
			names.push_back(s.entry->name);
	return names;
}

static const Status* Lookup(const char* name)
{
	if (!g_initialized)
	{
		log_error << "GameOffsets::Address(" << name << ") called before GameOffsets::Initialize()" << std::endl;
		return NULL;
	}
	std::map<std::string, size_t>::const_iterator it = g_index.find(orange::LowerCopy(name));
	if (it == g_index.end())
	{
		log_error << "GameOffsets: unknown offset name " << name << std::endl;
		return NULL;
	}
	return &g_status[it->second];
}

uintptr_t Rva(const char* name)
{
	const Status* s = Lookup(name);
	return s ? s->rva : 0;
}

uintptr_t Address(const char* name)
{
	const Status* s = Lookup(name);
	return (s && s->rva) ? g_base + s->rva : 0;
}

bool IsResolved(const char* name)
{
	const Status* s = Lookup(name);
	return s && s->rva != 0;
}

Source SourceOf(const char* name)
{
	const Status* s = Lookup(name);
	return s ? s->source : Source::Unresolved;
}

bool BuildAtLeast(int build)
{
	if (g_referenceBuild)
		return false;
	int current = GameBuildNumber();
	return current != 0 && current >= build;
}

bool WriteTemplate(const std::string& path)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out.good())
	{
		log_error << "Cannot write offsets template " << path << std::endl;
		return false;
	}
#ifndef ORANGE_VERSION
#define ORANGE_VERSION "dev"
#endif
	out << "; GTA:Orange offsets template, generated by orange-core " << ORANGE_VERSION << "\n";
	out << "; Game: " << g_gameExe << "\n";
	out << "; Version: " << g_gameVersion << ", image size " << HexString(g_imageSize) << "\n";
	out << "; Reference build: " << (g_referenceBuild ? "yes" : "no") << "\n";
	out << ";\n";
	out << "; Fill in the unresolved entries, rename the file to offsets.ini and put it\n";
	out << "; next to orange-core.dll. Every value is one of:\n";
	out << ";   Name = 0x1F26D4              RVA relative to the GTA5.exe base\n";
	out << ";   Name = disabled              skip this hook / patch\n";
	out << ";   Name = scan                  use the pattern built into orange-core\n";
	out << ";   Name = 48 8B ? ? E8 @ -7     scan for this pattern, add -7 to the match\n";
	out << "; Required entries keep orange-core inactive while unresolved; optional ones\n";
	out << "; are simply skipped. See docs/UPDATING_OFFSETS.md.\n";
	out << "\n[" << g_gameVersion << "]\n";

	for (int pass = 0; pass < 2; ++pass)
	{
		out << "\n; ---------------- " << (pass == 0 ? "required" : "optional") << " ----------------\n";
		for (const Status& s : g_status)
		{
			if (s.entry->required != (pass == 0))
				continue;
			out << "\n; " << s.entry->name << ": " << s.entry->comment << "\n";
			if (s.entry->referenceRva)
				out << ";   reference build: " << HexString(s.entry->referenceRva) << "\n";
			if (s.entry->pattern)
			{
				out << ";   built-in pattern(s): " << s.entry->pattern;
				if (!strchr(s.entry->pattern, '@'))
					out << " @ " << s.entry->patternDelta;
				out << "\n";
			}
			switch (s.source)
			{
			case Source::Disabled:
				out << s.entry->name << " = disabled\n";
				break;
			case Source::Unresolved:
				out << ";   UNRESOLVED (" << (s.entry->required ? "required" : "optional") << "): " << s.note << "\n";
				out << ";" << s.entry->name << " = 0x\n";
				break;
			default:
				out << ";   resolved via " << s.note << "\n";
				out << s.entry->name << " = " << HexString(s.rva) << "\n";
				break;
			}
		}
	}
	out << "\n";
	return out.good();
}

} // namespace GameOffsets
