#include "stdafx.h"

#pragma comment(lib, "winmm.lib")

ScriptManagerThread g_ScriptManagerThread;

static HANDLE		mainFiber;
static Script *		currentScript;
scriptMap			ScriptManagerThread::m_scripts;

void Script::Tick() 
{
	if (mainFiber == nullptr)
		mainFiber = ConvertThreadToFiber(nullptr);

	if (timeGetTime() < wakeAt)
		return;

	if (scriptFiber) 
	{
		currentScript = this;
		SwitchToFiber(scriptFiber);
		currentScript = nullptr;
	}
	else
	{
		scriptFiber = CreateFiber(NULL, [](LPVOID handler) {
			//__try {
				reinterpret_cast<Script*>(handler)->Run();
			/*}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				printStack(std::cout);
				log_error << "Error in script->Run. Callstack was written to " << std::endl;
			}*/
		}, this);
	}
}

void Script::Run()
{
	callbackFunction();
}

void Script::Yield(uint32_t time)
{
	wakeAt = timeGetTime() + time;
	SwitchToFiber(mainFiber);
}

// The game has booted once its own scripts took the loading screen down and
// the player is in the world. Read through natives from the script thread,
// which is the one place they are meant to be called from.
//
// Every probe value is logged now and then, because on a new game build the
// first question is whether natives execute at all: GET_FRAME_COUNT must be
// non-zero and growing and PLAYER_PED_ID non-zero, otherwise the invocation
// (context layout, handler lookup, crossmap) is broken and taking over would
// only freeze the game's own scripts for nothing.
static bool GameHasBooted()
{
	static unsigned ticks = 0;
	static int lastFrame = 0;
	static bool nativesReported = false;
	++ticks;
	bool loading = DLC2::GET_IS_LOADING_SCREEN_ACTIVE() != 0;
	int player = PLAYER::PLAYER_ID();
	int ped = PLAYER::PLAYER_PED_ID();
	bool playing = PLAYER::IS_PLAYER_PLAYING(player) != 0;
	int frame = GAMEPLAY::GET_FRAME_COUNT();
	bool pedExists = ped != 0 && ENTITY::DOES_ENTITY_EXIST(ped) != 0;
	int health = pedExists ? ENTITY::GET_ENTITY_HEALTH(ped) : 0;
	bool nativesAlive = frame != 0 && (frame != lastFrame || ticks == 1) && ped != 0;

	if (ticks == 1 || ticks % 900 == 0 || (nativesAlive && !nativesReported))
	{
		log_info << "Script thread: waiting for the game to boot (loading screen " << (loading ? "active" : "gone")
			<< ", player " << (playing ? "playing" : "not playing yet") << ", tick " << ticks << ") - natives: PLAYER_ID=" << player
			<< " PLAYER_PED_ID=" << ped << " DOES_ENTITY_EXIST=" << (pedExists ? 1 : 0) << " GET_ENTITY_HEALTH=" << health
			<< " GET_FRAME_COUNT=" << frame << (nativesAlive ? " (natives execute)" : " (natives return nothing yet)") << std::endl;
		if (nativesAlive && !nativesReported)
		{
			nativesReported = true;
			log_info << "Natives: the game executes them on this build (frame counter and player ped answer)" << std::endl;
		}
		if (!nativesAlive && ticks >= 1800)
			log_error << "Natives: still no answer after " << ticks << " ticks; the calls reach the handlers but nothing comes back. "
				"Check the natives-<version>.txt translations and the call context layout (Core/nativeInvoker.h)" << std::endl;
	}
	lastFrame = frame;

	// The loading screen is down and the player is in the world: either the
	// game says so, or (IS_PLAYER_PLAYING answered no while the ped is there,
	// seen on 1.0.3889.0 when injected into a running story mode) the ped
	// exists and is alive.
	if (!loading && nativesAlive && (playing || (pedExists && health > 0)))
		return true;
	return false;
}

void ScriptManagerThread::DoRun()
{
	// For the render thread (D3DHook::Render), which must not call natives.
	CGlobals::Get().pauseMenuActive = UI::IS_PAUSE_MENU_ACTIVE() != 0 || UI::_0xE18B138FABC53103() != 0;

	if (!ScriptEngine::TookOver())
	{
		if (!GameHasBooted())
			return;
		ScriptEngine::TakeOver();
	}
	for (auto & pair : m_scripts)
		pair.second->Tick();
}

eThreadState ScriptManagerThread::Reset(uint32_t scriptHash, void * pArgs, uint32_t argCount)
{
	scriptMap tempScripts;
	for (auto && pair : m_scripts)
		tempScripts[pair.first] = pair.second;
	m_scripts.clear();
	for (auto && pair : tempScripts)
		AddScript(pair.first, pair.second->GetCallbackFunction());
	return ScriptThread::Reset(scriptHash, pArgs, argCount);
}

void ScriptManagerThread::AddScript(std::string threadName, void(*fn)())
{
	log_debug << "Registering thread " << threadName.c_str() << " 0x" << std::hex << fn << std::dec << std::endl;
	if (m_scripts.find(threadName) != m_scripts.end()) 
	{
		log_error << "Thread " << threadName.c_str() << " is already registered" << std::endl;
		return;
	}
	m_scripts[threadName] = std::make_shared<Script>(fn);
}

void ScriptManagerThread::RemoveScript(std::string threadName) {

	auto pair = m_scripts.find(threadName);
	if (pair == m_scripts.end()) {
		log_error << "Could not find thread " << threadName << std::endl;
		return;
	}
	log_debug << "Unregistered script " << threadName << std::endl;
	m_scripts.erase(pair);
}

void scriptWait(unsigned long waitTime) 
{
	currentScript->Yield(waitTime);
}

void scriptRegister(std::string threadName, void(*function)()) 
{
	g_ScriptManagerThread.AddScript(threadName, function);
}

void scriptUnregister(std::string threadName)
{
	g_ScriptManagerThread.RemoveScript(threadName);
}

int32_t getGameVersion()
{
	log_info << "getGameVersion not realized yet" << std::endl;
	return 0;
}

static ScriptManagerContext g_context;
static uint64_t g_hash;

void nativePush64(UINT64 value)
{
	g_context.Push(value);
}

void nativeInit(UINT64 hash)
{
	g_context.Reset();
	g_hash = hash;
}

uint64_t * nativeCall()
{
	auto fn = ScriptEngine::GetNativeHandler(g_hash);
	if (fn != 0) {
		__try {
			fn(&g_context);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			log_error << "Error in nativeCall. 0x" << g_hash << std::endl;
		}
	}
	return reinterpret_cast<uint64_t*>(g_context.GetResultPointer());
}


static std::set<TKeyboardFn> g_keyboardFunctions;

void keyboardHandlerRegister(TKeyboardFn function)
{
	g_keyboardFunctions.insert(function);
}

void keyboardHandlerUnregister(TKeyboardFn function)
{
	g_keyboardFunctions.erase(function);
}

void ScriptManager::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_KEYDOWN || uMsg == WM_KEYUP || uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP)
	{
		auto functions = g_keyboardFunctions;
		for (auto & function : functions)
			function((DWORD)wParam, lParam & 0xFFFF, (lParam >> 16) & 0xFF, (lParam >> 24) & 1, (uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP), (lParam >> 30) & 1, (uMsg == WM_SYSKEYUP || uMsg == WM_KEYUP));
	}
}