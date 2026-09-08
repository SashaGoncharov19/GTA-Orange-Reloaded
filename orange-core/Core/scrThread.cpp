#include "stdafx.h"

// The GTA part of the thread object (script name, script handler, network
// flags) moved between game builds; ScriptEngine::ScriptHandlerOffset() reads
// the position of the script handler from the game's own Kill code and the
// network flag sits a constant 0x31 bytes behind it (0x110 / 0x141 on the
// reference build, 0x118 / 0x149 since 1.0.2699). See scrThread.h.
void ** ScriptThread::ScriptHandlerSlot()
{
	return (void**)((char*)this + ScriptEngine::ScriptHandlerOffset());
}

uint8_t * ScriptThread::NetworkFlagByte()
{
	return (uint8_t*)this + ScriptEngine::ScriptHandlerOffset() + 0x31;
}

void * ScriptThread::GetScriptHandler()
{
	return *ScriptHandlerSlot();
}

void ScriptThread::SetScriptNetworkFlag(bool value)
{
	*NetworkFlagByte() = value ? 1 : 0;
}

eThreadState ScriptThread::Tick(uint32_t opsToExecute)
{
	typedef eThreadState(__thiscall * ScriptThreadTick_t)(ScriptThread * ScriptThread, uint32_t opsToExecute);
	static ScriptThreadTick_t threadTickGta = GameFunc<ScriptThreadTick_t>("ScriptThreadTick");
	if (!threadTickGta)
		return m_Context.m_State;
	// With ScriptEngine::InstallHooks() in place this lands in the Tick hook,
	// which runs our thread through Run() and keeps the stock scripts frozen.
	return threadTickGta(this, opsToExecute);
}

void ScriptThread::Kill()
{
	typedef void(__thiscall * ScriptThreadKill_t)(ScriptThread * ScriptThread);
	static ScriptThreadKill_t killScriptThread = GameFunc<ScriptThreadKill_t>("ScriptThreadKill");
	if (!killScriptThread)
		return;
	return killScriptThread(this);
}

eThreadState ScriptThread::Run(uint32_t opsToExecute)
{
	if (GetScriptHandler() == nullptr)
	{
		static bool attachFailed = false;
		scriptHandlerMgr * mgr = ScriptEngine::GetScriptHandleMgr();
		if (mgr && !attachFailed)
		{
			mgr->AttachScript(this);
			if (GetScriptHandler() == nullptr)
			{
				// AttachScript did not fill the slot we read: the layout
				// assumption is wrong for this build, do not retry every frame.
				attachFailed = true;
				log_error << "ScriptThread: AttachScript left the script handler slot (thread+0x" << std::hex
					<< ScriptEngine::ScriptHandlerOffset() << std::dec << ") empty, layout mismatch; not retrying" << std::endl;
			}
			else
			{
				SetScriptNetworkFlag(true);
				log_info << "ScriptThread: script handler attached to thread " << GetId() << std::endl;
			}
		}
	}
	scrThread * activeThread = ScriptEngine::GetActiveThread();
	ScriptEngine::SetActiveThread(this);
	if (m_Context.m_State != ThreadStateKilled)
		DoRun();
	ScriptEngine::SetActiveThread(activeThread);
	return m_Context.m_State;
}

void ScriptThreadInit(ScriptThread * thread)
{
	typedef void(__thiscall * ScriptThreadInit_t)(ScriptThread * ScriptThread);
	static ScriptThreadInit_t ScriptThreadInit_ = GameFunc<ScriptThreadInit_t>("ScriptThreadInit");
	if (!ScriptThreadInit_)
	{
		log_error << "ScriptThreadInit unresolved, script thread not initialised" << std::endl;
		return;
	}
	return ScriptThreadInit_(thread);
}

eThreadState ScriptThread::Reset(uint32_t scriptHash, void* pArgs, uint32_t argCount)
{
	memset(&m_Context, 0, sizeof(m_Context));
	memset(_gtaThreadData, 0, sizeof(_gtaThreadData));

	m_Context.m_State = ThreadStateIdle;
	m_Context.m_iScriptHash = scriptHash;
	m_Context.m_iUnk1 = -1;
	m_Context.m_iUnk2 = -1;
	m_Context.m_iSet1 = 1;

	// The game's own initialisation of the GTA part (network id, flags, ...).
	ScriptThreadInit(this);

	m_pszExitMessage = "Normal exit";
	ScriptEngine::SetThreadName(this, "gta_orange");
	return m_Context.m_State;
}
