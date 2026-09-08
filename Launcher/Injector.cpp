#include "stdafx.h"

Injector * Injector::instance = nullptr;

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
		throw std::runtime_error("Can't start executable");
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
		MessageBox(NULL, L"GTA5.exe is not running", L"GTA:Orange Launcher", MB_OK | MB_ICONERROR);
		return false;
	}
	if (waitForUnpack)
		WaitForUnpackFinished(pid, unpackTimeoutSeconds);
	for (const std::string& lib : libs)
	{
		std::string error;
		if (!Inject(pid, lib, error))
		{
			std::string message = "Failed to inject " + lib + "\n" + error;
			MessageBoxA(NULL, message.c_str(), "GTA:Orange Launcher", MB_OK | MB_ICONERROR);
			return false;
		}
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
	while (FindProcess(PROCESS_NAME) == -1)
	{
		if (timeoutSeconds > 0 && GetTickCount64() > deadline)
			return false;
		Sleep(250);
	}
	return true;
}

bool Injector::Inject(int processId, std::string dllName, std::string& error)
{
	HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, false, processId);
	if (!process)
	{
		error = "OpenProcess failed (error " + std::to_string(GetLastError()) + ")";
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
		error = "LoadLibrary failed inside the game process";
		return false;
	}
	return true;
}

// The retail executable is packed; wait until its code section has been
// rewritten in memory before patching anything. Gives up after the timeout.
bool Injector::WaitForUnpackFinished(int pid, int timeoutSeconds)
{
	HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (!process)
		return false;

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
			CloseHandle(process);
			return false;
		}
	}
}


Injector::~Injector()
{
}
