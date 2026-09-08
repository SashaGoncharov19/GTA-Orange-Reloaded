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
		// features (direct connect UI, relaxed game build check).
		std::fstream isDev(CGlobals::Get().orangePath + "/orange.developer");
		if (isDev.good())
			CGlobals::Get().isDeveloper = true;
		isDev.close();

		my_ostream::SetLogFile(CGlobals::Get().orangePath + "/client.log");
		log_info << "orange-core loaded from " << CGlobals::Get().orangePath << std::endl;

		if (!PreLoadPatches())
		{
			log_error << "Game build check failed, GTA:Orange stays inactive" << std::endl;
			MessageBoxA(NULL,
				"This GTA V build is not supported by this orange-core.dll.\n"
				"GTA:Orange stays inactive. See client.log next to the DLL for details.",
				"GTA:Orange", MB_OK | MB_ICONERROR);
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
