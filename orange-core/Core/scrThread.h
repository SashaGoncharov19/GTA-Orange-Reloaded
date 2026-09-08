#pragma once
enum eThreadState {
	ThreadStateIdle,
	ThreadStateRunning,
	ThreadStateKilled,
	ThreadState3,
	ThreadState4,
};

// scrThreadContext: 168 (0xA8) bytes, stable across builds since the reference
// build. Lives at +8 in a thread object (right after the vtable pointer).
class scrThreadContext {
public:

	uint32_t					m_iThreadId; 		//0x0000 
	uint32_t					m_iScriptHash; 		//0x0004 
	eThreadState				m_State; 			//0x0008 
	uint32_t					m_iIP; 				//0x000C 
	uint32_t					m_iFrameSP; 		//0x0010 
	uint32_t					m_iSP; 				//0x0014 
	uint32_t					m_iTimerA; 			//0x0018 
	uint32_t					m_iTimerB; 			//0x001C 
	uint32_t					m_iTimerC; 			//0x0020 
	uint32_t					m_iUnk1; 			//0x0024 
	uint32_t					m_iUnk2;			//0x0028 
	char _0x002C[52];
	uint32_t					m_iSet1;			//0x0060 
	char _0x0064[68];
};

class scrThread {
protected:

	scrThreadContext			m_Context;			//0x0008
	__int64						m_pStack;			//0x00B0
	char _0x00B8[16];
	char *						m_pszExitMessage;	//0x00C8

public:

	virtual ~scrThread() {}
	virtual eThreadState		Reset(uint32_t scriptHash, void* pArgs, uint32_t argCount) = 0;
	virtual eThreadState		Run(uint32_t opsToExecute) = 0;
	virtual eThreadState		Tick(uint32_t opsToExecute) = 0;
	virtual void				Kill() = 0;

	inline scrThreadContext *	GetContext() { return &m_Context; }
	inline uint32_t				GetId() { return m_Context.m_iThreadId; }
};

// The GTA-specific part of a script thread object (the "GtaThread") starts at
// +0xD0 and its members moved between the reference build (script handler at
// +0x110) and current builds (2699+: +0x118). Rather than hard-code one
// layout, ScriptThread reserves a generous zeroed buffer so that the game's
// own initialise / tick / kill code (which writes up to +0x15C on 1.0.3889.0)
// always stays inside the object, and reaches the two fields orange-core
// itself touches through the runtime offset ScriptEngine::ScriptHandlerOffset()
// (the network flag sits a constant 0x31 bytes after the handler on every
// build seen so far). See Core/scrThread.cpp and docs/PORTING_STATUS.md.
static const size_t kScriptThreadSize = 0x2C0;   // >= any known GtaThread size, with margin

class ScriptThread : public scrThread {
private:
	char _gtaThreadData[kScriptThreadSize - 0xD0];   // 0xD0 = sizeof(scrThread)

	void ** ScriptHandlerSlot();
	uint8_t * NetworkFlagByte();

public:
	virtual void				DoRun() = 0;
	virtual eThreadState		Reset(uint32_t scriptHash, void* pArgs, uint32_t argCount);
	virtual eThreadState		Run(uint32_t opsToExecute);
	virtual eThreadState		Tick(uint32_t opsToExecute);
	virtual void				Kill();

	void *						GetScriptHandler();
	void						SetScriptNetworkFlag(bool value);
};
