// dllmain.cpp: DLL entry point of orange-core (injected into GTA5.exe by the Launcher).
#include "stdafx.h"

std::string my_ostream::fname = "output.log";

std::string GetModuleDir()
{
	HMODULE hModule;
	char    cPath[MAX_PATH] = { 0 };
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)GetModuleDir, &hModule);

	GetModuleFileNameA(hModule, cPath, MAX_PATH);
	std::string path = cPath;
	return path.substr(0, path.find_last_of("\\/"));
}

static bool FileExists(const std::string& path)
{
	DWORD attributes = GetFileAttributesA(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

// The MinHook based hooks suspend the other threads while they are written,
// which must not happen under the loader lock: they are installed from here,
// right after DllMain has returned.
static DWORD WINAPI InstallHooksThread(LPVOID)
{
	InstallGameHooks();
	return 0;
}

// On a build other than the reference one, write the natives the game
// registered (build hash + handler RVA) next to the DLL: the raw material
// for natives-<version>.txt (docs/PORTING_STATUS.md). Runs on its own thread
// a moment after injection, never inside the loader lock.
static DWORD WINAPI DumpNativesThread(LPVOID)
{
	Sleep(3000);
	std::string path = CGlobals::Get().orangePath + "\\natives-" + GameOffsets::GameVersion() + ".registered.txt";
	NativeTable::DumpRegistered(path);
	return 0;
}

static void StartThread(LPTHREAD_START_ROUTINE routine, const char* what)
{
	HANDLE thread = CreateThread(NULL, 0, routine, NULL, 0, NULL);
	if (thread)
		CloseHandle(thread);
	else
		log_error << "CreateThread failed for " << what << " (error " << GetLastError() << ")" << std::endl;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		CGlobals::Get().dllModule = hModule;
		CGlobals::Get().orangePath = GetModuleDir();

		// An empty "orange.developer" file next to the DLL enables developer
		// features (direct connect UI, relaxed game build check);
		// "orange.storymode" lets the game's own scripts keep running.
		CGlobals::Get().isDeveloper = FileExists(CGlobals::Get().orangePath + "\\orange.developer");
		CGlobals::Get().storyMode = FileExists(CGlobals::Get().orangePath + "\\orange.storymode");

		my_ostream::SetLogFile(CGlobals::Get().orangePath + "/client.log");
#ifndef ORANGE_VERSION
#define ORANGE_VERSION "dev"
#endif
		log_info << "orange-core " << ORANGE_VERSION << " loaded from " << CGlobals::Get().orangePath
			<< (CGlobals::Get().isDeveloper ? " (developer mode)" : "") << (CGlobals::Get().storyMode ? " (story mode)" : "") << std::endl;

		bool patched = PreLoadPatches();
		if (GameOffsets::IsInitialized() && !GameOffsets::IsReferenceBuild())
			StartThread(DumpNativesThread, "the natives dump");
		if (patched)
			StartThread(InstallHooksThread, "the hook installation");
		else
		{
			log_error << "Game build check failed, GTA:Orange stays inactive" << std::endl;
			std::string message =
				"This GTA V build (version " + GameOffsets::GameVersion() + ") is not supported by this orange-core.dll:\n"
				+ std::to_string(GameOffsets::UnresolvedRequired().size()) + " required game offset(s) are unknown.\n\n"
				"GTA:Orange stays inactive. See client.log and offsets-" + GameOffsets::GameVersion() + ".generated.ini\n"
				"next to the DLL, and docs/UPDATING_OFFSETS.md for how to add the offsets.";
			MessageBoxA(NULL, message.c_str(), "GTA:Orange", MB_OK | MB_ICONERROR);
		}
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		FreeConsole();
		break;
	}
	return TRUE;
}
