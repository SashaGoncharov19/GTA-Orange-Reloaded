#pragma once

class scriptHandlerMgr 
{
public:
	virtual ~scriptHandlerMgr();
	virtual void _Function1() = 0;
	virtual void _Function2() = 0;
	virtual void _Function3() = 0;
	virtual void _Function4() = 0;
	virtual void _Function5() = 0;
	virtual void _Function6() = 0;
	virtual void _Function7() = 0;
	virtual void _Function8() = 0;
	virtual void _Function9() = 0;
	virtual void AttachScript(scrThread * thread) = 0;
	virtual void DetachScript(scrThread * thread) = 0;
};

class ScriptEngine 
{
public:
	static bool Initialize();
	static pgPtrCollection<ScriptThread> * GetThreadCollection();
	static scriptHandlerMgr * GetScriptHandleMgr();
	static scrThread * GetActiveThread();
	static void SetActiveThread(scrThread * thread);
	static void CreateThread(ScriptThread * thread);
	typedef void(__cdecl * NativeHandler)(scrNativeCallContext * context);
	static NativeHandler GetNativeHandler(uint64_t oldHash);

	// Offset of the script handler pointer inside a thread object on the
	// running build (read from the game's Kill code; 0x110 on the reference
	// build, 0x118 since 1.0.2699).
	static int ScriptHandlerOffset();
	// Writes the script name (and, on 1.0.2699+, its hash) into the thread object.
	static void SetThreadName(ScriptThread * thread, const char * name);
	static bool IsOwnedThread(scrThread * thread);
	// True when the stock single player scripts may run (orange.storymode).
	static bool StockScriptsAllowed();
	// True once the game has allocated its script thread collection.
	static bool ThreadCollectionReady();
	// MinHook hooks on the script engine (thread tick, script id comparison).
	static bool InstallHooks();
};
