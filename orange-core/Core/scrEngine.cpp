#include "stdafx.h"

static pgPtrCollection<ScriptThread> * scrThreadCollection;
static uint32_t activeThreadTlsOffset;
static uint32_t * scrThreadId;
static uint32_t * scrThreadCount;

static scriptHandlerMgr * g_scriptHandlerMgr;

struct NativeRegistration 
{
	NativeRegistration * nextRegistration;
	ScriptEngine::NativeHandler handlers[7];
	uint32_t numEntries;
	uint64_t hashes[7];
};

static NativeRegistration ** registrationTable;
static std::unordered_set<ScriptThread*> g_ownedThreads;
static std::unordered_map<uint64_t, uint64_t> foundHashCache;
static eGameState * gameState;

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
	auto getScriptIdBlock = GameMem("GetScriptIdBlock");

	scrThreadCollection = reinterpret_cast<decltype(scrThreadCollection)>(scrThreadCollectionPattern.getOffset());
	if (scrThreadCollection == nullptr)
	{
		log_error << "Unable to find scrThreadCollection" << std::endl;
		return false;
	}
	log_debug << "scrThreadCollection\t " << std::hex << scrThreadCollection << std::endl;

	uint32_t * tlsLoc = activeThreadTlsOffsetPattern.get<uint32_t>(0);
	if (tlsLoc == nullptr)
	{
		log_error << "Unable to find activeThreadTlsOffset" << std::endl;
		return false;
	}
	activeThreadTlsOffset = *tlsLoc;
	log_debug << "activeThreadTlsOffset " << std::hex << activeThreadTlsOffset << std::endl;

	// Get thread id
	scrThreadId = reinterpret_cast<decltype(scrThreadId)>(scrThreadIdPattern.getOffset(2));
	if (scrThreadId == nullptr)
	{
		log_error << "Unable to find scrThreadId" << std::endl;
		return false;
	}
	log_debug << "scrThreadId\t\t " << std::hex << scrThreadId << std::endl;

	scrThreadCount = reinterpret_cast<decltype(scrThreadCount)>(scrThreadCountPattern.getOffset(2));
	if (scrThreadCount == nullptr)
	{
		log_error << "Unable to find scrThreadCount" << std::endl;
		return false;
	}
	log_debug << "scrThreadCount\t " << std::hex << scrThreadCount << std::endl;

	registrationTable = reinterpret_cast<decltype(registrationTable)>(registrationTablePattern.getOffset());
	if (registrationTable == nullptr)
	{
		log_error << "Unable to find registrationTable" << std::endl;
		return false;
	}
	log_debug << "registrationTable\t " << std::hex << registrationTable << std::endl;
	NativeTable::Initialize();

	g_scriptHandlerMgr = reinterpret_cast<decltype(g_scriptHandlerMgr)>(g_scriptHandlerMgrPattern.getOffset());
	if (g_scriptHandlerMgr == nullptr)
	{
		log_error << "Unable to find g_scriptHandlerMgr" << std::endl;
		return false;
	}
	log_debug << "g_scriptHandlerMgr\t " << std::hex << g_scriptHandlerMgr << std::endl;

	void * script_location = getScriptIdBlock.get<void>(4);
	if (script_location == nullptr)
	{
		log_error << "Unable to find getScriptIdBlock" << std::endl;
		return false;
	}

	// ERR_SYS_PURE
	static uint8_t block[2] = { 0xEB };
	unsigned long OldProtection;
	VirtualProtect(script_location, 2, PAGE_EXECUTE_READWRITE, &OldProtection);
	memcpy(&block, script_location, 2);
	VirtualProtect(script_location, 2, OldProtection, NULL);
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
	int slot = 0;

	for (auto & thread : *collection)
	{
		auto context = thread->GetContext();
		if (context->m_iThreadId == 0)
			break;
		slot++;
	}

	if (slot == collection->count())
		return;

	auto context = thread->GetContext();
	thread->Reset((*scrThreadCount) + 1, nullptr, 0);

	if (*scrThreadId == 0)
		(*scrThreadId)++;

	context->m_iThreadId = *scrThreadId;

	(*scrThreadCount)++;
	(*scrThreadId)++;

	collection->set(slot, thread);

	g_ownedThreads.insert(thread);

	log_debug << "Created thread, id " << thread->GetId() << std::endl;
}

// `hash` is the canonical hash (Natives.h, Lua); NativeTable translates it to
// the running build and walks the plain or obfuscated registration table.
ScriptEngine::NativeHandler ScriptEngine::GetNativeHandler(uint64_t hash)
{
	return NativeTable::Lookup(hash);
}
