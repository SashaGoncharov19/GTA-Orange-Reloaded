#include "stdafx.h"

static pgPtrCollection<ScriptThread> * scrThreadCollection;
static uint32_t activeThreadTlsOffset;
static uint32_t * scrThreadId;
static uint32_t * scrThreadCount;
static scriptHandlerMgr * g_scriptHandlerMgr;
static void ** registrationTable;
static std::unordered_set<ScriptThread*> g_ownedThreads;
static int g_scriptHandlerOffset = 0;

// GtaThread::Kill begins with "mov [rsp+8],rbx; push rdi; sub rsp,20h;
// cmp qword ptr [rcx+X],0" (48 83 B9 disp32 00 at +0xA): X is where the
// script handler pointer lives. SEH-guarded, no C++ objects (C2712).
static bool ReadHandlerDisplacement(const uint8_t * kill, int32_t * out)
{
	__try
	{
		if (kill[0xA] == 0x48 && kill[0xB] == 0x83 && kill[0xC] == 0xB9)
		{
			*out = *(const int32_t*)(kill + 0xD);
			return true;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
	return false;
}

int ScriptEngine::ScriptHandlerOffset()
{
	if (g_scriptHandlerOffset)
		return g_scriptHandlerOffset;
	int32_t disp = 0;
	const uint8_t * kill = GameFunc<const uint8_t*>("ScriptThreadKill");
	if (kill && ReadHandlerDisplacement(kill, &disp) && disp >= 0xD0 && disp + 0x40 <= (int32_t)kScriptThreadSize)
	{
		g_scriptHandlerOffset = disp;
		log_info << "ScriptEngine: script handler at thread+0x" << std::hex << disp << std::dec << " (read from ScriptThreadKill)" << std::endl;
	}
	else
	{
		g_scriptHandlerOffset = GameOffsets::BuildAtLeast(2699) ? 0x118 : 0x110;
		log_info << "ScriptEngine: script handler offset not readable from ScriptThreadKill, assuming thread+0x"
			<< std::hex << g_scriptHandlerOffset << std::dec << " for this build" << std::endl;
	}
	return g_scriptHandlerOffset;
}

void ScriptEngine::SetThreadName(ScriptThread * thread, const char * name)
{
	char * base = (char*)thread;
	char * dest;
	if (ScriptHandlerOffset() >= 0x118)
	{
		// 1.0.2699+: uint32 script name hash at +0xD0, the name at +0xD4
		*(uint32_t*)(base + 0xD0) = Utils::Hash(name);
		dest = base + 0xD4;
	}
	else
		dest = base + 0xD0;   // reference build: the name follows scrThread directly
	strncpy_s(dest, 64, name, _TRUNCATE);
}

bool ScriptEngine::IsOwnedThread(scrThread * thread)
{
	return g_ownedThreads.find((ScriptThread*)thread) != g_ownedThreads.end();
}

bool ScriptEngine::StockScriptsAllowed()
{
	return CGlobals::Get().storyMode;
}

bool ScriptEngine::ThreadCollectionReady()
{
	if (!scrThreadCollection)
		scrThreadCollection = reinterpret_cast<decltype(scrThreadCollection)>(GameMem("ScrThreadCollection").getOffset());
	return scrThreadCollection && scrThreadCollection->begin() != nullptr && scrThreadCollection->count() > 0;
}

bool ScriptEngine::Initialize()
{
	log_info << "Initializing ScriptEngine..." << std::endl;
	// All addresses come from GameOffsets (offsets.ini / reference build / pattern scan).
	auto scrThreadCollectionPattern = GameMem("ScrThreadCollection");
	auto activeThreadTlsOffsetPattern = GameMem("ActiveThreadTlsOffset");
	auto scrThreadIdPattern = GameMem("ScrThreadId");
	auto scrThreadCountPattern = GameMem("ScrThreadCount");
	auto registrationTablePattern = GameMem("RegistrationTable");
	auto g_scriptHandlerMgrPattern = GameMem("ScriptHandlerMgr");

	scrThreadCollection = reinterpret_cast<decltype(scrThreadCollection)>(scrThreadCollectionPattern.getOffset());
	if (scrThreadCollection == nullptr)
	{
		log_error << "Unable to find scrThreadCollection" << std::endl;
		return false;
	}
	log_debug << "scrThreadCollection\t " << std::hex << scrThreadCollection << std::dec << " (" << scrThreadCollection->count() << " thread slots)" << std::endl;

	uint32_t * tlsLoc = activeThreadTlsOffsetPattern.get<uint32_t>(0);
	if (tlsLoc == nullptr)
	{
		log_error << "Unable to find activeThreadTlsOffset" << std::endl;
		return false;
	}
	activeThreadTlsOffset = *tlsLoc;
	log_debug << "activeThreadTlsOffset " << std::hex << activeThreadTlsOffset << std::dec << std::endl;

	scrThreadId = reinterpret_cast<decltype(scrThreadId)>(scrThreadIdPattern.getOffset(2));
	if (scrThreadId == nullptr)
	{
		log_error << "Unable to find scrThreadId" << std::endl;
		return false;
	}
	log_debug << "scrThreadId\t\t " << std::hex << scrThreadId << std::dec << std::endl;

	scrThreadCount = reinterpret_cast<decltype(scrThreadCount)>(scrThreadCountPattern.getOffset(2));
	if (scrThreadCount == nullptr)
	{
		log_error << "Unable to find scrThreadCount" << std::endl;
		return false;
	}
	log_debug << "scrThreadCount\t " << std::hex << scrThreadCount << std::dec << std::endl;

	registrationTable = reinterpret_cast<decltype(registrationTable)>(registrationTablePattern.getOffset());
	if (registrationTable == nullptr)
	{
		log_error << "Unable to find registrationTable" << std::endl;
		return false;
	}
	log_debug << "registrationTable\t " << std::hex << registrationTable << std::dec << std::endl;
	NativeTable::Initialize();

	g_scriptHandlerMgr = reinterpret_cast<decltype(g_scriptHandlerMgr)>(g_scriptHandlerMgrPattern.getOffset());
	if (g_scriptHandlerMgr == nullptr)
	{
		log_error << "Unable to find g_scriptHandlerMgr" << std::endl;
		return false;
	}
	log_debug << "g_scriptHandlerMgr\t " << std::hex << g_scriptHandlerMgr << std::dec << std::endl;

	ScriptHandlerOffset();
	return true;
}

scriptHandlerMgr * ScriptEngine::GetScriptHandleMgr()
{
	return g_scriptHandlerMgr;
}

pgPtrCollection<ScriptThread>* ScriptEngine::GetThreadCollection()
{
	return scrThreadCollection;
}

scrThread * ScriptEngine::GetActiveThread()
{
	char * moduleTls = *(char**)__readgsqword(88);
	return *reinterpret_cast<scrThread**>(moduleTls + activeThreadTlsOffset);
}

void ScriptEngine::SetActiveThread(scrThread * thread)
{
	char * moduleTls = *(char**)__readgsqword(88);
	*reinterpret_cast<scrThread**>(moduleTls + activeThreadTlsOffset) = thread;
}

void ScriptEngine::CreateThread(ScriptThread * thread)
{
	auto collection = GetThreadCollection();
	if (!collection || !scrThreadCount || !scrThreadId)
	{
		log_error << "CreateThread: script engine not initialised" << std::endl;
		return;
	}
	if (g_ownedThreads.find(thread) != g_ownedThreads.end() && thread->GetContext()->m_iThreadId != 0)
	{
		log_debug << "CreateThread: thread " << thread->GetId() << " already exists" << std::endl;
		return;
	}

	int slot = 0;
	for (auto & entry : *collection)
	{
		auto context = entry->GetContext();
		if (context->m_iThreadId == 0)
			break;
		slot++;
	}

	if (slot == collection->count())
	{
		log_error << "CreateThread: no free slot in the script thread collection (" << collection->count() << " entries)" << std::endl;
		return;
	}

	auto context = thread->GetContext();
	thread->Reset((*scrThreadCount) + 1, nullptr, 0);

	if (*scrThreadId == 0)
		(*scrThreadId)++;

	context->m_iThreadId = *scrThreadId;

	(*scrThreadCount)++;
	(*scrThreadId)++;

	collection->set(slot, thread);

	g_ownedThreads.insert(thread);

	log_info << "Created script thread, id " << thread->GetId() << " in slot " << slot << std::endl;
}

// `hash` is the canonical hash (Natives.h, Lua); NativeTable translates it to
// the running build and walks the plain or obfuscated registration table.
ScriptEngine::NativeHandler ScriptEngine::GetNativeHandler(uint64_t hash)
{
	return NativeTable::Lookup(hash);
}

// ---------------------------------------------------------------------------
// Hooks (the FiveM way, rage-scripting-five/src/scrEngine.cpp)
// ---------------------------------------------------------------------------

// GtaThread::Tick: our own threads run through Run(); every other thread is
// left in its current state, so the stock single player scripts never
// execute (unless orange.storymode allows them).
typedef eThreadState(*ThreadTick_t)(ScriptThread * thread, uint32_t opsToExecute);
static ThreadTick_t g_origThreadTick = nullptr;

static eThreadState ThreadTickHook(ScriptThread * thread, uint32_t opsToExecute)
{
	if (g_ownedThreads.find(thread) != g_ownedThreads.end())
		return thread->Run(0);
	if (CGlobals::Get().storyMode)
		return g_origThreadTick(thread, opsToExecute);
	return thread->GetContext()->m_State;
}

// Script id comparison ("may this script use this entity"): always yes while
// the stock scripts are disabled, so entities created by our thread can be
// used from any script context.
typedef int(*ScriptIdCompare_t)(void * a, void * b);
static ScriptIdCompare_t g_origScriptIdCompare = nullptr;

static int ScriptIdCompareHook(void * a, void * b)
{
	if (CGlobals::Get().storyMode)
		return g_origScriptIdCompare(a, b);
	return 1;
}

bool ScriptEngine::InstallHooks()
{
	bool ok = true;
	if (GameOffsets::IsResolved("ScriptThreadTick"))
	{
		if (HookGameFunction("ScriptThreadTick", (void*)ThreadTickHook, (void**)&g_origThreadTick))
			log_info << "Script hooks: stock scripts " << (CGlobals::Get().storyMode ? "allowed (orange.storymode)" : "frozen, only GTA:Orange threads run") << std::endl;
		else
			ok = false;
	}
	else
		ok = false;
	if (GameOffsets::IsResolved("ScriptIdCompare"))
		HookGameFunction("ScriptIdCompare", (void*)ScriptIdCompareHook, (void**)&g_origScriptIdCompare);
	else
		log_info << "Script hooks: ScriptIdCompare unresolved, entity ownership checks stay as they are" << std::endl;
	return ok;
}
