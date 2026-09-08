#pragma once

enum GameVersion {
	GAME_VERSION_STEAM = 1,
	GAME_VERSION_SOCIAL = 2,
	GAME_VERSION_RELOADED = 3
};

class Injector
{
	// ctor & dtor
	Injector();
	~Injector();

	// methods
	int FindProcess(std::wstring procName);
	GameVersion GetGameVersion();
	bool Inject(int processId, std::string dllName, std::string& error);
	bool WaitForUnpackFinished(int pid, int timeoutSeconds);

	// static fields
	static Injector * instance;

	// fields
	std::vector<std::string> libs;
	bool Injected = false;
public:
	// methods
	void Run(std::wstring folder, std::wstring pePath);
	void RunSteam();
	// Waits until GTA5.exe shows up in the process list. Returns false on timeout.
	bool WaitUntilGameStarts(int timeoutSeconds);
	// Injects all pushed libraries into the running GTA5.exe. Returns false on failure.
	bool InjectAll(bool waitForUnpack = true, int unpackTimeoutSeconds = 120);
	void PushLibrary(std::string path);


	// static methods
	static Injector& Get() {
		if (!instance)
			instance = new Injector();
		return *instance;
	}
};
