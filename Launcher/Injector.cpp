#include "stdafx.h"
#include "LauncherLog.h"

Injector * Injector::instance = nullptr;

static std::string LastErrorText()
{
	return "error " + std::to_string(GetLastError());
}

// Anti-cheat clients (BattlEye: BEClient_x64.dll) block DLL injection by
// design. GTA:Orange only works in story mode with the anti-cheat turned off,
// which the Rockstar Games Launcher offers as a setting; this just reports it.
static bool GameHasAntiCheatModule(int pid, std::string& moduleName)
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (snapshot == INVALID_HANDLE_VALUE)
		return false;
	bool found = false;
	MODULEENTRY32 entry;
	entry.dwSize = sizeof(entry);
	if (Module32First(snapshot, &entry))
	{
		do {
			if (_wcsnicmp(entry.szModule, L"BEClient", 8) == 0)
			{
				moduleName = Utils::UnicodeToMultibyte(entry.szModule);
				found = true;
				break;
			}
		} while (Module32Next(snapshot, &entry));
	}
	CloseHandle(snapshot);
	return found;
}

Injector::Injector()
{
}

void Injector::Run(std::wstring folder, std::wstring pePath)
{
	TCHAR Params[] = L"";
	STARTUPINFO siStartupInfo;
	PROCESS_INFORMATION piProcessInfo;
	memset(&siStartupInfo, 0, sizeof(siStartupInfo));
	memset(&piProcessInfo, 0, sizeof(piProcessInfo));
	siStartupInfo.cb = sizeof(siStartupInfo);
	if (!CreateProcess(pePath.c_str(), Params, NULL, NULL, true, CREATE_SUSPENDED, NULL, folder.c_str(), &siStartupInfo, &piProcessInfo))
		throw std::runtime_error("Can't start GTA5.exe (" + LastErrorText() + ")");
	LauncherLog("GTA5.exe started, pid " + std::to_string(piProcessInfo.dwProcessId));
	ResumeThread(piProcessInfo.hThread);
	CloseHandle(piProcessInfo.hThread);
	CloseHandle(piProcessInfo.hProcess);
}

void Injector::RunSteam()
{
	ShellExecute(NULL, NULL, L"steam://run/271590", NULL, NULL, SW_SHOW);
}

bool Injector::InjectAll(bool waitForUnpack, int unpackTimeoutSeconds)
{
	Sleep(100);
	int pid = FindProcess(PROCESS_NAME);
	if (pid == -1)
	{
		LauncherLog("inject: GTA5.exe is not running");
		MessageBox(NULL, L"GTA5.exe is not running", L"GTA:Orange Launcher", MB_OK | MB_ICONERROR);
		return false;
	}
	LauncherLog("inject: GTA5.exe pid " + std::to_string(pid));
	std::string antiCheat;
	if (GameHasAntiCheatModule(pid, antiCheat))
		LauncherLog("inject: WARNING: anti-cheat module " + antiCheat + " is loaded in GTA5.exe, injecting will be refused while it runs; "
			"GTA:Orange needs story mode with BattlEye turned off (a setting of the Rockstar Games Launcher)");
	if (waitForUnpack)
	{
		LauncherLog("inject: waiting for the executable to be unpacked (up to " + std::to_string(unpackTimeoutSeconds) + "s)");
		if (WaitForUnpackFinished(pid, unpackTimeoutSeconds))
			LauncherLog("inject: code section changed, executable unpacked");
		else
			LauncherLog("inject: unpack wait timed out or the process could not be read, injecting anyway");
	}
	else
		LauncherLog("inject: not waiting for unpack");
	for (const std::string& lib : libs)
	{
		std::string error;
		LauncherLog("inject: loading " + lib);
		if (!Inject(pid, lib, error))
		{
			LauncherLog("inject: FAILED: " + error);
			std::string message = "Failed to inject " + lib + "\n" + error + "\n\nSee launcher.log and client.log next to Launcher.exe.";
			MessageBoxA(NULL, message.c_str(), "GTA:Orange Launcher", MB_OK | MB_ICONERROR);
			return false;
		}
		LauncherLog("inject: " + lib + " loaded");
	}
	Injected = true;
	return true;
}

void Injector::PushLibrary(std::string path)
{
	if (Injected == true)
		throw std::runtime_error("Libraries are already injected");
	if (!Utils::FileExist(Utils::MultibyteToUnicode(path)))
		throw std::runtime_error("Library doesn't exist: " + path);
	libs.push_back(path);
}

int Injector::FindProcess(std::wstring procName)
{
	HANDLE hSnap = INVALID_HANDLE_VALUE;
	PROCESSENTRY32 ProcessStruct;
	ProcessStruct.dwSize = sizeof(PROCESSENTRY32);
	hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE)
		return -1;
	if (Process32First(hSnap, &ProcessStruct) == FALSE)
	{
		CloseHandle(hSnap);
		return -1;
	}
	do {
		if (_wcsicmp(ProcessStruct.szExeFile, procName.c_str()) == 0) {
			CloseHandle(hSnap);
			return ProcessStruct.th32ProcessID;
		}
	} while (Process32Next(hSnap, &ProcessStruct));
	CloseHandle(hSnap);
	return -1;
}

GameVersion Injector::GetGameVersion()
{
	return GameVersion();
}

bool Injector::WaitUntilGameStarts(int timeoutSeconds)
{
	ULONGLONG deadline = GetTickCount64() + (ULONGLONG)timeoutSeconds * 1000ULL;
	int pid;
	while ((pid = FindProcess(PROCESS_NAME)) == -1)
	{
		if (timeoutSeconds > 0 && GetTickCount64() > deadline)
		{
			LauncherLog("GTA5.exe did not show up within " + std::to_string(timeoutSeconds) + "s");
			return false;
		}
		Sleep(250);
	}
	LauncherLog("GTA5.exe is running, pid " + std::to_string(pid));
	return true;
}

bool Injector::Inject(int processId, std::string dllName, std::string& error)
{
	HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, false, processId);
	if (!process)
	{
		DWORD code = GetLastError();
		error = "OpenProcess failed (error " + std::to_string(code) + ")";
		if (code == ERROR_ACCESS_DENIED)
			error += ": access denied - the game runs in another Proton prefix / as another user, or an anti-cheat protects it";
		return false;
	}
	LPVOID LoadLibraryA_ = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");
	size_t size = dllName.length() + 1;
	LPVOID LoadComp = VirtualAllocEx(process, NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!LoadComp)
	{
		error = "VirtualAllocEx failed (error " + std::to_string(GetLastError()) + ")";
		CloseHandle(process);
		return false;
	}
	WriteProcessMemory(process, LoadComp, dllName.c_str(), size, NULL);
	HANDLE injectThread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA_, LoadComp, 0, NULL);
	if (!injectThread)
	{
		error = "CreateRemoteThread failed (error " + std::to_string(GetLastError()) + ")";
		VirtualFreeEx(process, LoadComp, 0, MEM_RELEASE);
		CloseHandle(process);
		return false;
	}
	WaitForSingleObject(injectThread, INFINITE);
	DWORD exitCode = 0;
	GetExitCodeThread(injectThread, &exitCode);
	VirtualFreeEx(process, LoadComp, 0, MEM_RELEASE);
	CloseHandle(injectThread);
	CloseHandle(process);
	if (exitCode == 0)
	{
		// LoadLibraryA returned NULL inside the game: missing dependency, wrong
		// architecture or the DLL refused to load (see client.log).
		error = "LoadLibrary failed inside the game process (missing dependency or the DLL refused to load; check client.log)";
		return false;
	}
	char handleText[32];
	snprintf(handleText, sizeof(handleText), "0x%lX", (unsigned long)exitCode);
	LauncherLog(std::string("inject: LoadLibrary returned ") + handleText + " (module handle, truncated to 32 bits)");
	return true;
}

// The retail executable is packed; wait until its code section has been
// rewritten in memory before patching anything. Gives up after the timeout.
bool Injector::WaitForUnpackFinished(int pid, int timeoutSeconds)
{
	HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (!process)
	{
		LauncherLog("unpack wait: OpenProcess failed (" + LastErrorText() + ")");
		return false;
	}

	ULONGLONG deadline = GetTickCount64() + (ULONGLONG)timeoutSeconds * 1000ULL;
	HMODULE hMod = NULL;
	while (hMod == NULL)
	{
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
		if (snapshot != INVALID_HANDLE_VALUE)
		{
			MODULEENTRY32 ModEnt;
			ModEnt.dwSize = sizeof(MODULEENTRY32);
			if (Module32First(snapshot, &ModEnt))
			{
				do {
					if (_wcsicmp(PROCESS_NAME, ModEnt.szModule) == 0)
					{
						hMod = ModEnt.hModule;
						break;
					}
				} while (Module32Next(snapshot, &ModEnt));
			}
			CloseHandle(snapshot);
		}
		if (hMod == NULL)
		{
			if (GetTickCount64() > deadline)
			{
				LauncherLog("unpack wait: GTA5.exe module not found in the process within the timeout");
				CloseHandle(process);
				return false;
			}
			Sleep(50);
		}
	}

	unsigned char buff[10] = { 0 };
	ReadProcessMemory(process, (LPVOID)((uint64_t)hMod + 0x1000), buff, 10, NULL);
	for (;;)
	{
		Sleep(5);
		unsigned char newBuff[10] = { 0 };
		ReadProcessMemory(process, (LPVOID)((uint64_t)hMod + 0x1000), newBuff, 10, NULL);
		for (int i = 0; i < 10; ++i)
		{
			if (buff[i] != newBuff[i])
			{
				CloseHandle(process);
				return true;
			}
		}
		if (GetTickCount64() > deadline)
		{
			LauncherLog("unpack wait: code section did not change within the timeout");
			CloseHandle(process);
			return false;
		}
	}
}


Injector::~Injector()
{
}
