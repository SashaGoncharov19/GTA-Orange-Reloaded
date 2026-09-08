#include "stdafx.h"

// Every GTA5.exe address used below comes from GameOffsets (GameOffsets.cpp),
// looked up by name. GameMem("Name") returns an inert CMemory (address 0,
// writes skipped) when the entry is unresolved on the running game build, so
// the optional patches simply do nothing there.
//
// Two hook mechanisms:
//  * MinHook function hooks (the FiveM way) for the per-frame message pump
//    (LookAlive), the script startup (StartupScript) and the script engine
//    (ScriptEngine::InstallHooks). They need no spare bytes in the game and
//    are installed from a thread after DllMain (InstallGameHooks).
//  * call-site redirects for the reference build's E8 / FF 15 sites
//    (LookAliveCall, GameStateChangeCall, WindowCreateCall, BeginDisplayCall):
//    the rel32 of the call is pointed at a 12-byte far jump in a "code cave",
//    the CodeCave offset or a page allocated within 2 GB of GTA5.exe. They are
//    plain memory writes and happen in PreLoadPatches (DllMain), before the
//    game reaches the patched code.

bool ScriptsDisabled = false;

typedef int(*LookAliveFn)();
typedef bool(*GameStateChangeFn)(int gameState);
typedef void(*StartupScriptFn)();
typedef void(*DrawTextManager__BeginDisplay_)(int64_t, int64_t);

static LookAliveFn g_origLookAlive = nullptr;
static GameStateChangeFn g_origGameStateChange = nullptr;
static StartupScriptFn g_origStartupScript = nullptr;
static DrawTextManager__BeginDisplay_ DrawTextManager__BeginDisplay = nullptr;

static bool g_gameReady = false;
static bool g_readyTriggerInstalled = false;
static bool g_minHookInitialized = false;

// ---------------------------------------------------------------------------
// MinHook
// ---------------------------------------------------------------------------
bool HookAddress(void* target, void* detour, void** original, const char* what)
{
	if (!target)
	{
		log_error << "Hook " << what << ": target unresolved" << std::endl;
		return false;
	}
	if (!g_minHookInitialized)
	{
		MH_STATUS init = MH_Initialize();
		if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED)
		{
			log_error << "MinHook: MH_Initialize failed (" << (int)init << ")" << std::endl;
			return false;
		}
		g_minHookInitialized = true;
	}
	MH_STATUS status = MH_CreateHook(target, detour, original);
	if (status != MH_OK)
	{
		log_error << "Hook " << what << ": MH_CreateHook failed (" << (int)status << ") at 0x" << std::hex << (uintptr_t)target << std::dec << std::endl;
		return false;
	}
	status = MH_EnableHook(target);
	if (status != MH_OK)
	{
		log_error << "Hook " << what << ": MH_EnableHook failed (" << (int)status << ")" << std::endl;
		MH_RemoveHook(target);
		return false;
	}
	log_info << "Hook " << what << ": installed at 0x" << std::hex << (uintptr_t)target << std::dec << std::endl;
	return true;
}

bool HookGameFunction(const char* offsetName, void* detour, void** original)
{
	return HookAddress((void*)GameOffsets::Address(offsetName), detour, original, offsetName);
}

// ---------------------------------------------------------------------------
// Code cave for the call-site redirects. A 5-byte E8 call cannot reach the
// DLL, so it is pointed at a 12-byte far jump that lives within 2 GB of the
// call site: the CodeCave offset when it is known, otherwise a page
// allocated as close to GTA5.exe as the address space allows.
// ---------------------------------------------------------------------------
static uint8_t* g_cave = nullptr;
static size_t g_caveLeft = 0;

static void* AllocateNear(uintptr_t base, size_t size)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	uintptr_t gran = si.dwAllocationGranularity ? si.dwAllocationGranularity : 0x10000;
	const uintptr_t reach = 0x7FF00000;   // just under 2 GB
	uintptr_t lowest = (uintptr_t)si.lpMinimumApplicationAddress;
	uintptr_t highest = (uintptr_t)si.lpMaximumApplicationAddress;
	uintptr_t low = (base > lowest + reach) ? base - reach : lowest;
	uintptr_t high = (base + reach < highest) ? base + reach : highest;
	if (low < gran)
		low = gran;

	// Below the module first (the usual free space), then above it.
	uintptr_t addr = base & ~(gran - 1);
	for (int guard = 0; guard < 1000000 && addr > low + gran; ++guard)
	{
		addr -= gran;
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)))
			break;
		if (mbi.State == MEM_FREE)
		{
			uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
			if (regionEnd >= addr + size)
			{
				void* p = VirtualAlloc((LPVOID)addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
				if (p)
					return p;
			}
		}
		else if ((uintptr_t)mbi.BaseAddress < addr)
			addr = (uintptr_t)mbi.BaseAddress & ~(gran - 1);   // skip the rest of this region
	}
	addr = base & ~(gran - 1);
	for (int guard = 0; guard < 1000000 && addr + gran + size < high; ++guard)
	{
		addr += gran;
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)))
			break;
		uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
		if (mbi.State == MEM_FREE)
		{
			if (regionEnd >= addr + size)
			{
				void* p = VirtualAlloc((LPVOID)addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
				if (p)
					return p;
			}
		}
		else if (regionEnd > addr + gran)
			addr = ((regionEnd + gran - 1) & ~(gran - 1)) - gran;   // next step lands at the region end
	}
	return nullptr;
}

static uint8_t* CodeCaveAlloc(size_t bytes)
{
	if (!g_cave)
	{
		CMemory cave = GameMem("CodeCave");
		uintptr_t base = (uintptr_t)GetModuleHandle(NULL);
		if (cave.valid())
		{
			g_cave = (uint8_t*)cave();
			g_caveLeft = 48;
			log_info << "Code cave: 48 bytes at GTA5.exe+0x" << std::hex << (cave() - base) << std::dec << " (CodeCave offset)" << std::endl;
		}
		else
		{
			void* page = AllocateNear(base, 4096);
			if (!page)
			{
				log_error << "Code cave: no executable memory within 2 GB of GTA5.exe could be allocated" << std::endl;
				return nullptr;
			}
			g_cave = (uint8_t*)page;
			g_caveLeft = 4096;
			log_info << "Code cave: allocated 4096 bytes at 0x" << std::hex << (uintptr_t)page << std::dec
				<< " (" << ((uintptr_t)page < base ? "below" : "above") << " GTA5.exe)" << std::endl;
		}
	}
	if (g_caveLeft < bytes)
	{
		log_error << "Code cave: out of space (" << bytes << " bytes requested, " << g_caveLeft << " left)" << std::endl;
		return nullptr;
	}
	uint8_t* p = g_cave;
	g_cave += bytes;
	g_caveLeft -= bytes;
	return p;
}

// Points the rel32 of the E8 call at `site` to a far jump to `target`.
// Returns the original call target (NULL when the site is unresolved).
template <typename T>
static T RedirectCall(CMemory site, void* target)
{
	if (!site.valid())
		return (T)0;
	uint8_t* stub = CodeCaveAlloc(12);
	if (!stub)
		return (T)0;
	T original = site.get_call<T>();
	CMemory((UINT64)stub).farJmp(target);
	(site + 1).put(DWORD((uintptr_t)stub - site() - 5));
	return original;
}

// Replaces the 6-byte "FF 15 rel32" (call [import]) at `site` by a 5-byte
// call into the code cave plus a nop.
static bool RedirectImportCall(CMemory site, void* target)
{
	if (!site.valid())
		return false;
	uint8_t* stub = CodeCaveAlloc(12);
	if (!stub)
		return false;
	CMemory((UINT64)stub).farJmp(target);
	site.nearCall(DWORD((uintptr_t)stub - site() - 5));
	site.nop(1);
	return true;
}

// ---------------------------------------------------------------------------
// Byte patches
// ---------------------------------------------------------------------------
void ForceToSingle()
{
	CMemory mem = GameMem("ForceToSingle");
	CMemory mem2 = GameMem("ForceToSingle_2");
	if (!mem.valid() || !mem2.valid())
	{
		log_info << "ForceToSingle skipped: unresolved on this build" << std::endl;
		return;
	}
	// The rel32 of the jmp at +0x3A is pointed at ForceToSingle_2.
	(mem + 0x3B).put(DWORD(mem2() - mem() - 0x3F));
}

void UnknownPatches()
{
	CMemory mem = GameMem("UnknownPatch_1");
	if (mem.valid())
	{
		CMemory mem2 = mem + 13;
		mem2.put(0x01B0i16);   // mov al, 1
		mem2.nop(3);
	}
	GameMem("UnknownPatch_2").nop(5);
	GameMem("UnknownPatch_3").retn();
	GameMem("UnknownPatch_4").retn();
	GameMem("UnknownPatch_5").retn();
	GameMem("UnknownPatch_6").nop(9);
	GameMem("UnknownPatch_7").retn();
	GameMem("UnknownPatch_8").retn();

	mem = GameMem("CheckMultiplayerByteDrawMapFrame");
	if (mem.valid())
	{
		CMemory mem2 = CMemory(mem);
		mem.nop(7);
		mem2.put(0xB640i16);   // mov sil, 1
		mem2.put(0x01i8);
	}

	GameMem("UnknownPatch_9").put(0x08i8);
	GameMem("UnknownPatch_10").retn();
}

void DefineNatives()
{
	CGlobals::Get().ForceCleanupForAllThreadsWithThisName =
		GameFunc<ForceCleanupForAllThreadsWithThisName_>("ForceCleanupForAllThreadsWithThisName");
	CGlobals::Get().TerminateAllScriptsWithThisName =
		GameFunc<TerminateAllScriptsWithThisName_>("TerminateAllScriptsWithThisName");
	CGlobals::Get().ShutdownLoadingScreen = GameFunc<ShutdownLoadingScreen_>("ShutdownLoadingScreen");
	CGlobals::Get().DoScreenFadeIn = GameFunc<DoScreenFadeIn_>("DoScreenFadeIn");
	CGlobals::Get().HasScriptLoaded = GameFunc<HasScriptLoaded_>("HasScriptLoaded");
	CGlobals::Get().InitializeOnline = GameFunc<InitializeOnline_>("InitializeOnline");

	// The flag lives one byte after the global referenced by the instruction.
	static bool fallbackCanLangChange = false;
	LPVOID langGlobal = GameMem("CanLangChange").getOffset(2);
	CGlobals::Get().canLangChange = langGlobal ? (bool*)((uintptr_t)langGlobal + 1) : &fallbackCanLangChange;
}

void GameProcessHooks()
{
	GameMem("ObjectsPatch").nop(24);
	GameMem("EscFreeze").nop(5);
	GameMem("CheatConsole").retn();
	GameMem("UIWheelSlowmo").nop(6);
	GameMem("ShowCursor_1").nop(4);
	GameMem("ShowCursor_2").nop(4);
	GameMem("RockstarLoadingLogo").retn();
	GameMem("Tooltips").retn();
	GameMem("SocialClubNews").retn();
	GameMem("DisableWantedGeneration_1").retn();
	GameMem("DisableWantedGeneration_2").put(0xE990i16);
	GameMem("IntentionalCrash").nop(5);
	GameMem("CrashLoadModelsTooQuickly").nop(23);
	GameMem("CreateNetworkEventBindings").retn();
	GameMem("LoadNewGame").retn();
	GameMem("ResetVehicleDensityLastFrame").retn();
	uint64_t* vehicleDensity = (uint64_t*)GameMem("VarVehicleDensity").getOffset(2);
	if (vehicleDensity)
		*vehicleDensity = 0;
	GameMem("SetClockForwardAfterDeath").retn();
	GameMem("DisableNorthBlip").nop(46);
	GameMem("DisableVehicleResetAtSetPosition").retn();
	GameMem("DisableLoadingMpDlcContent").retn();
	GameMem("RuntimeExecutableImportsCheck").retn();
	GameMem("DisablePopulationVehicles_10").retn();
	GameMem("DisablePopulationVehicles_8").retn();
	GameMem("DisablePopulationVehicles_11a").nop(3);
	GameMem("DisablePopulationVehicles_11b").nop(5);
	GameMem("DisablePopulationPeds_1").retn();
	GameMem("DisablePopulationPeds_2").retn();
	CMemory mem = GameMem("DisablePopulationAmbientPeds");
	(mem + 6).put(0x0i32);
	(mem + 16).put(0x0i32);
	GameMem("DisablePopulationPeds_4").nop(20);
	GameMem("DisableCopsAndFireTrucks_1").retn();
	GameMem("DisableCopsAndFireTrucks_2").retn();
	GameMem("DisableCopsAndFireTrucks_3").retn();
}

// ---------------------------------------------------------------------------
// Window / input
// ---------------------------------------------------------------------------
void TurnOnConsole()
{
	AllocConsole();
	SetConsoleTitle(L"Grand Theft Auto: Orange");
	FILE * unused = NULL;
	freopen_s(&unused, "CONOUT$", "w", stdout);
	freopen_s(&unused, "CONOUT$", "w", stderr);
}

LRESULT APIENTRY WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ScriptManager::WndProc(hwnd, uMsg, wParam, lParam);
	ImGui_ImplDX11_WndProcHandler(hwnd, uMsg, wParam, lParam);
	return CallWindowProc(CGlobals::Get().gtaWndProc, hwnd, uMsg, wParam, lParam);
}

static BOOL CALLBACK FindGameWindowProc(HWND hwnd, LPARAM param)
{
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid != GetCurrentProcessId() || GetWindow(hwnd, GW_OWNER) != NULL || !IsWindowVisible(hwnd))
		return TRUE;
	HWND* found = (HWND*)param;
	wchar_t className[64] = { 0 };
	GetClassNameW(hwnd, className, 64);
	if (wcscmp(className, L"grcWindow") == 0)
	{
		*found = hwnd;
		return FALSE;
	}
	if (!*found)
		*found = hwnd;   // first visible top-level window of the process as a fallback
	return TRUE;
}

static HWND FindGameWindow()
{
	HWND found = NULL;
	EnumWindows(FindGameWindowProc, (LPARAM)&found);
	return found;
}

void AttachInputHook()
{
	if (CGlobals::Get().gtaWndProc)
		return;
	if (!CGlobals::Get().gtaHwnd)
		CGlobals::Get().gtaHwnd = FindGameWindow();
	if (!CGlobals::Get().gtaHwnd)
	{
		log_error << "Input hook: the game window is not known yet" << std::endl;
		return;
	}
	CGlobals::Get().gtaWndProc = (WNDPROC)SetWindowLongPtr(CGlobals::Get().gtaHwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
	if (CGlobals::Get().gtaWndProc == NULL)
		log_error << "Failed to attach input hook (SetWindowLongPtr error " << GetLastError() << ")" << std::endl;
	else
		log_info << "Input hook attached: WndProc 0x" << std::hex << (DWORD_PTR)CGlobals::Get().gtaWndProc << std::dec << std::endl;
}

static HWND CreateWindowExWHook(_In_ DWORD dwExStyle,
	_In_opt_ LPCWSTR lpClassName,
	_In_opt_ LPCWSTR lpWindowName,
	_In_ DWORD dwStyle,
	_In_ int X,
	_In_ int Y,
	_In_ int nWidth,
	_In_ int nHeight,
	_In_opt_ HWND hWndParent,
	_In_opt_ HMENU hMenu,
	_In_opt_ HINSTANCE hInstance,
	_In_opt_ LPVOID lpParam)
{
	Icon = (LPARAM)LoadIcon(CGlobals::Get().dllModule,MAKEINTRESOURCE(IDI_ICON1));
	HWND hWnd = CreateWindowExW(dwExStyle, lpClassName, L"GTA:Orange", dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	SendMessage(hWnd, WM_SETICON, ICON_BIG, Icon);
	SendMessage(hWnd, WM_SETICON, ICON_SMALL, Icon);
	CGlobals::Get().gtaHwnd = hWnd;
	return hWnd;
}

static void BeginDisplayEx(int64_t textMgr, int64_t vp)
{
	if (DrawTextManager__BeginDisplay)
		DrawTextManager__BeginDisplay(textMgr, vp);
}

void __fastcall eventHook(GTA::CTask* task)
{
	/*log_debug << task->GetTree() << std::endl;
	CLocalPlayer::Get()->updateTasks = true;*/
}

// ---------------------------------------------------------------------------
// Game ready: what used to run at GameStatePlaying
// ---------------------------------------------------------------------------
static bool ScriptsUsable()
{
	return GameOffsets::IsReferenceBuild() || NativeTable::TranslationCount() > 0;
}

// Writes the natives the running build registered (build hash + handler RVA)
// next to the DLL: the raw material for a crossmap, and the proof that the
// registration table was found. Only meaningful once the game is ready - at
// injection time (first loading screen) the table is still empty.
static DWORD WINAPI DumpNativesThread(LPVOID)
{
	std::string path = CGlobals::Get().orangePath + "\\natives-" + GameOffsets::GameVersion() + ".registered.txt";
	NativeTable::DumpRegistered(path);
	return 0;
}

void OnGameReady()
{
	if (g_gameReady)
		return;
	g_gameReady = true;
	CGlobals::Get().currentGameState = GameStatePlaying;
	log_info << "Game ready: initialising GTA:Orange" << std::endl;

	if (!ScriptEngine::Initialize())
		log_error << "Failed to initialize ScriptEngine" << std::endl;
	if (!D3DHook::HookD3D11())
		log_error << "Failed to hook D3D11, the chat and UI will not render" << std::endl;
	CChat::Get()->RegisterCommandProcessor(CommandProcessor);
	AttachInputHook();

	if (ScriptsUsable())
	{
		ScriptEngine::CreateThread(&g_ScriptManagerThread);
		CScript::RunAll();
	}
	else
	{
		log_error << "Scripts not started: no natives crossmap for game version " << GameOffsets::GameVersion()
			<< " (natives-" << GameOffsets::GameVersion() << ".txt next to orange-core.dll). Start the game through Launcher.exe with "
			"internet access once, or run tools/natives/crossmap_from_fivem.py; see docs/PORTING_STATUS.md" << std::endl;
		CChat::Get()->AddChatMessage("{FF8F00}GTA:Orange{FFFFFF}: no natives crossmap for game version " + GameOffsets::GameVersion()
			+ " (natives-" + GameOffsets::GameVersion() + ".txt is missing next to orange-core.dll), scripts are not started. See client.log.");
	}

	GameMem("EventHook").farJmp(eventHook);

	if (!GameOffsets::IsReferenceBuild())
	{
		HANDLE thread = CreateThread(NULL, 0, DumpNativesThread, NULL, 0, NULL);
		if (thread)
			CloseHandle(thread);
	}
	log_info << "Game ready: done" << std::endl;
}

// ---------------------------------------------------------------------------
// Per frame (the game's main thread)
// ---------------------------------------------------------------------------
static void OnGameFrame()
{
	static bool HUDInited = false;
	if (!HUDInited)
	{
		typedef void(*InitHUD)(void);
		InitHUD initHud = GameFunc<InitHUD>("InitHUD");
		if (initHud)
			initHud();
		HUDInited = true;
	}

	// No startup-script / game-state hook on this build: the game is ready
	// once the local player ped exists and the script thread collection is
	// allocated.
	if (!g_gameReady && !g_readyTriggerInstalled)
	{
		CWorld* world = CWorld::Get();
		if (world && world->CPedPtr && ScriptEngine::ThreadCollectionReady())
		{
			log_info << "Game ready: detected through the local player ped" << std::endl;
			OnGameReady();
		}
	}

	// Reference build route: stop the single player scripts by name once they
	// start, then leave the loading screen. On builds without those functions
	// the stock scripts never tick (ScriptEngine::InstallHooks) and the script
	// thread leaves the loading screen through natives.
	if (g_gameReady && !IsScriptsDisabled() && CanDisableScriptsByName() && IsAnyScriptLoaded())
	{
		DisableScripts();
		if (CGlobals::Get().ShutdownLoadingScreen)
			CGlobals::Get().ShutdownLoadingScreen();
		if (CGlobals::Get().DoScreenFadeIn)
			CGlobals::Get().DoScreenFadeIn(0);
	}
}

static int LookAliveHook()
{
	OnGameFrame();
	return g_origLookAlive ? g_origLookAlive() : 0;
}

static void StartupScriptHook()
{
	log_info << "StartupScript hook: the game is about to start its startup script" << std::endl;
	OnGameReady();
	if (g_origStartupScript)
		g_origStartupScript();
}

static bool GameStateChangeHook(int gameState)
{
	if (gameState == GameStatePlaying)
		OnGameReady();
	CGlobals::Get().currentGameState = gameState;
	return g_origGameStateChange ? g_origGameStateChange(gameState) : false;
}

// ---------------------------------------------------------------------------
// Hook installation
// ---------------------------------------------------------------------------

// Call-site redirects (plain memory writes, done in DllMain like the original
// patches). Only for sites whose MinHook alternative is unresolved.
static void InstallCallSiteHooks()
{
	if (!GameOffsets::IsResolved("LookAlive") && GameOffsets::IsResolved("LookAliveCall"))
	{
		g_origLookAlive = RedirectCall<LookAliveFn>(GameMem("LookAliveCall"), (void*)LookAliveHook);
		if (g_origLookAlive)
			log_info << "Per-frame hook: LookAliveCall call site redirected" << std::endl;
	}
	if (!GameOffsets::IsResolved("StartupScript") && GameOffsets::IsResolved("GameStateChangeCall"))
	{
		g_origGameStateChange = RedirectCall<GameStateChangeFn>(GameMem("GameStateChangeCall"), (void*)GameStateChangeHook);
		if (g_origGameStateChange)
		{
			g_readyTriggerInstalled = true;
			log_info << "Game ready trigger: GameStateChangeCall call site redirected" << std::endl;
		}
	}
	if (GameOffsets::IsResolved("WindowCreateCall") && RedirectImportCall(GameMem("WindowCreateCall"), (void*)CreateWindowExWHook))
		log_info << "Window hook: CreateWindowExW call site redirected" << std::endl;
	if (GameOffsets::IsResolved("BeginDisplayCall"))
		DrawTextManager__BeginDisplay = RedirectCall<DrawTextManager__BeginDisplay_>(GameMem("BeginDisplayCall"), (void*)BeginDisplayEx);
}

// MinHook hooks; called on a worker thread once DllMain has returned.
void InstallGameHooks()
{
	if (!GameOffsets::IsInitialized())
		return;

	if (GameOffsets::IsResolved("LookAlive"))
	{
		if (HookGameFunction("LookAlive", (void*)LookAliveHook, (void**)&g_origLookAlive))
			log_info << "Per-frame hook: LookAlive function (MinHook)" << std::endl;
		else if (GameOffsets::IsResolved("LookAliveCall"))
			g_origLookAlive = RedirectCall<LookAliveFn>(GameMem("LookAliveCall"), (void*)LookAliveHook);
	}
	if (!g_origLookAlive)
		log_error << "No per-frame hook: neither LookAlive nor LookAliveCall is usable; HUD init, the ready poll and the stock script shutdown will not run" << std::endl;

	if (GameOffsets::IsResolved("StartupScript"))
	{
		if (HookGameFunction("StartupScript", (void*)StartupScriptHook, (void**)&g_origStartupScript))
		{
			g_readyTriggerInstalled = true;
			log_info << "Game ready trigger: StartupScript function (MinHook)" << std::endl;
		}
		else if (GameOffsets::IsResolved("GameStateChangeCall"))
		{
			g_origGameStateChange = RedirectCall<GameStateChangeFn>(GameMem("GameStateChangeCall"), (void*)GameStateChangeHook);
			g_readyTriggerInstalled = g_origGameStateChange != nullptr;
		}
	}
	if (!g_readyTriggerInstalled)
	{
		if (GameOffsets::IsResolved("World") && g_origLookAlive)
			log_info << "Game ready trigger: polling the local player ped (World) from the per-frame hook" << std::endl;
		else
			log_error << "No game ready trigger: StartupScript, GameStateChangeCall and the World poll are all unavailable; GTA:Orange will not initialise" << std::endl;
	}

	ScriptEngine::InstallHooks();
	log_info << "Game hooks installed" << std::endl;
}

// Resolves every game offset for the running GTA5.exe and applies the
// startup patches. Returns false (and touches nothing) when a required
// offset is unknown for this game build, unless orange.developer exists.
bool PreLoadPatches()
{
	if (!GameOffsets::Initialize())
	{
		std::vector<std::string> missing = GameOffsets::UnresolvedRequired();
		std::string list;
		for (size_t i = 0; i < missing.size(); ++i)
			list += (i ? ", " : "") + missing[i];
		log_error << missing.size() << " required offset(s) unknown for game version "
			<< GameOffsets::GameVersion() << ": " << list << std::endl;
		log_error << "Fill them in offsets.ini (a template was written next to orange-core.dll), see docs/UPDATING_OFFSETS.md" << std::endl;
		if (!CGlobals::Get().isDeveloper)
			return false;
		log_error << "orange.developer present, applying patches anyway (this will most likely crash the game)" << std::endl;
	}

	ImGui::GetIO().IniFilename = (CGlobals::Get().orangePath + "\\imgui.ini").c_str();
	ImGui::GetIO().LogFilename = (CGlobals::Get().orangePath + "\\imgui_log.txt").c_str();

	GameMem("StartupPatch").put(0xEB90909090909090);

	DefineNatives();
	ForceToSingle();
	UnknownPatches();
	InstallCallSiteHooks();
	GameProcessHooks();
	log_info << "Game patches applied" << std::endl;
	return true;
}
