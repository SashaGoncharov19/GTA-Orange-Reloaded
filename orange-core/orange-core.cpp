#include "stdafx.h"

// Every GTA5.exe address used below comes from GameOffsets (GameOffsets.cpp),
// looked up by name. GameMem("Name") returns an inert CMemory (address 0,
// writes skipped) when the entry is unresolved on the running game build, so
// the optional patches simply do nothing there. Required entries are checked
// once in PreLoadPatches() before anything is touched.

bool ScriptsDisabled = false;

void ForceToSingle()
{
	CMemory mem = GameMem("ForceToSingle");
	CMemory mem2 = GameMem("ForceToSingle_2");
	if (!mem.valid() || !mem2.valid())
	{
		log_error << "ForceToSingle skipped: offsets unresolved" << std::endl;
		return;
	}
	// The rel32 of the call/jmp at +0x3A is pointed at ForceToSingle_2.
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

static bool OnLookAlive()
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
	if (!IsScriptsDisabled() && IsAnyScriptLoaded())
	{
		DisableScripts();
		if (CGlobals::Get().ShutdownLoadingScreen)
			CGlobals::Get().ShutdownLoadingScreen();
		if (CGlobals::Get().DoScreenFadeIn)
			CGlobals::Get().DoScreenFadeIn(0);
	}
	//OnGameFrame
	return g_origLookAlive();
}


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

void __fastcall eventHook(GTA::CTask* task)
{
	/*log_debug << task->GetTree() << std::endl;
	CLocalPlayer::Get()->updateTasks = true;*/
}

bool consoleShowed = false;
void OnGameStateChange(int gameState)
{
	switch (gameState)
	{
	case GameStateIntro:
		break;
	case GameStateLicenseShit:
		break;
	case GameStatePlaying:
	{
		//TurnOnConsole();
		if (!ScriptEngine::Initialize())
			log_error << "Failed to initialize ScriptEngine" << std::endl;
		if (!D3DHook::HookD3D11())
			log_error << "Failed to hook D3D11, the chat and UI will not render" << std::endl;
		CChat::Get()->RegisterCommandProcessor(CommandProcessor);

		log_info << "Game ready" << std::endl;
		CGlobals::Get().gtaWndProc = (WNDPROC)SetWindowLongPtr(CGlobals::Get().gtaHwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
		if (CGlobals::Get().gtaWndProc == NULL)
			log_error << "Failed to attach input hook" << std::endl;
		else
			log_info << "Input hook attached: WndProc 0x" << std::hex << (DWORD_PTR)CGlobals::Get().gtaWndProc << std::endl;
		ScriptEngine::CreateThread(&g_ScriptManagerThread);
		CScript::RunAll();

#ifdef ORANGE_WITH_SCALEFORM
		auto text = rage::ScaleformManager::CreateText("Test", { 0,0,200,200 }, NULL);
#endif

		//SyncTree::Init();
		//log_debug << "CPlayerSyncTree: 0x" << std::hex << SyncTree::GetPlayerSyncTree() << std::endl;

		GameMem("EventHook").farJmp(eventHook);
		break;
	}
	case GameStateMainMenu:
		break;
	}
}

static bool gameStateChange_(int gameState)
{
	OnGameStateChange(gameState);
	CGlobals::Get().currentGameState = gameState;
	return g_gameStateChange();
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

typedef void(*DrawTextManager__BeginDisplay_)(int64_t, int64_t);
DrawTextManager__BeginDisplay_ DrawTextManager__BeginDisplay;

static void BeginDisplayEx(int64_t textMgr, int64_t vp)
{
	if (DrawTextManager__BeginDisplay)
		DrawTextManager__BeginDisplay(textMgr, vp);
}

// Redirects four call sites of the game through far jumps written into a
// code cave (unused executable memory inside GTA5.exe), because a 5-byte
// call cannot reach the DLL directly.
void HookLoop()
{
	CMemory unusedMem = GameMem("CodeCave");
	if (!unusedMem.valid())
	{
		log_error << "HookLoop: CodeCave unresolved, no game loop hooks installed" << std::endl;
		return;
	}

	CMemory lookFrame = GameMem("LookAliveCall");
	if (lookFrame.valid())
	{
		uintptr_t callToMem = unusedMem();
		unusedMem.farJmp(OnLookAlive);
		g_origLookAlive = lookFrame.get_call<LookAlive>();
		(lookFrame + 1).put(DWORD(callToMem - lookFrame() - 5));
	}
	else
		log_error << "HookLoop: LookAliveCall unresolved" << std::endl;

	CMemory gameStateChange = GameMem("GameStateChangeCall");
	if (gameStateChange.valid())
	{
		uintptr_t callToMem = unusedMem();
		unusedMem.farJmp(gameStateChange_);
		g_gameStateChange = gameStateChange.get_call<GameStateChange_>();
		(gameStateChange + 1).put(DWORD(callToMem - gameStateChange() - 5));
	}
	else
		log_error << "HookLoop: GameStateChangeCall unresolved" << std::endl;

	CMemory windowCreate = GameMem("WindowCreateCall");
	if (windowCreate.valid())
	{
		uintptr_t callToMem = unusedMem();
		unusedMem.farJmp(CreateWindowExWHook);
		uintptr_t windowCreateMem = windowCreate();
		windowCreate.nearCall(DWORD(callToMem - windowCreateMem - 5));
		windowCreate.nop(1);
	}
	else
		log_error << "HookLoop: WindowCreateCall unresolved" << std::endl;

	CMemory beginDisplay = GameMem("BeginDisplayCall");
	if (beginDisplay.valid())
	{
		uintptr_t callToMem = unusedMem();
		unusedMem.farJmp(BeginDisplayEx);
		DrawTextManager__BeginDisplay = beginDisplay.get_call<DrawTextManager__BeginDisplay_>();
		beginDisplay.nearCall(DWORD(callToMem - beginDisplay() - 5));
	}
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
	HookLoop();
	GameProcessHooks();
	log_info << "Game patches applied" << std::endl;
	return true;
}
