// Launcher.cpp: определяет точку входа для приложения.
//

#include "stdafx.h"
#include "Launcher.h"
#include "Injector.h"
#include <thread>

#define MAX_LOADSTRING 100
#define PIRATE_EXECUTABLE_SIZE 54528512

// Глобальные переменные:
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING] = L"GTA:Orange Launcher";
WCHAR szWindowClass[MAX_LOADSTRING] = L"_gtaorange_launcher";
Image* pBitmap = NULL;
float loadProgress = 0.0;
HWND splashHwnd = NULL;
ULONG_PTR m_gdiplusToken;


ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void				UpdateSplash(float progress);

void UpdateSplash(float progress)
{
	if (progress == 1.0)
	{
		TerminateProcess(GetCurrentProcess(), 0);
		return;
	}
	loadProgress = progress;
	::RECT rect;
	rect.left = 0;
	rect.top = 200;
	rect.right = 300;
	rect.bottom = 226;
	InvalidateRect(splashHwnd, &rect, false);
}

// ---------------------------------------------------------------------------
// Command line options
//
//   Launcher.exe                      start the game and inject (default)
//   Launcher.exe --inject             attach to an already running GTA5.exe
//                                     (used on Linux/Proton, see tools/proton)
//   Launcher.exe --game-dir <path>    GTA V folder, skips the folder dialog
//   Launcher.exe --steam / --direct   force the way the game is started
//   Launcher.exe --no-unpack-wait     do not wait for the exe to be unpacked
//   Launcher.exe --timeout <seconds>  how long to wait for GTA5.exe
// ---------------------------------------------------------------------------
struct LaunchOptions
{
	bool injectOnly = false;
	bool waitForUnpack = true;
	bool forceSteam = false;
	bool forceDirect = false;
	int timeoutSeconds = 600;
	std::wstring gameDir;
};
static LaunchOptions g_options;

static void ShowUsage()
{
	MessageBoxW(NULL,
		L"GTA:Orange Launcher\n\n"
		L"Launcher.exe [options]\n\n"
		L"  --inject            do not start the game, wait for a running GTA5.exe and inject\n"
		L"  --game-dir <path>   GTA V installation folder (skips the folder dialog)\n"
		L"  --steam             start the game through Steam (steam://run/271590)\n"
		L"  --direct            start GTA5.exe directly\n"
		L"  --no-unpack-wait    do not wait for the executable to be unpacked before injecting\n"
		L"  --timeout <sec>     how long to wait for GTA5.exe (default 600)\n"
		L"  --help              show this message",
		L"GTA:Orange Launcher", MB_OK | MB_ICONINFORMATION);
}

static bool ParseCommandLine(LaunchOptions& options)
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argv)
		return true;
	bool ok = true;
	for (int i = 1; i < argc && ok; ++i)
	{
		std::wstring arg = argv[i];
		if (arg == L"--inject" || arg == L"-i")
			options.injectOnly = true;
		else if (arg == L"--no-unpack-wait")
			options.waitForUnpack = false;
		else if (arg == L"--steam")
			options.forceSteam = true;
		else if (arg == L"--direct")
			options.forceDirect = true;
		else if ((arg == L"--game-dir" || arg == L"--gamedir") && i + 1 < argc)
			options.gameDir = argv[++i];
		else if (arg == L"--timeout" && i + 1 < argc)
			options.timeoutSeconds = _wtoi(argv[++i]);
		else if (arg == L"--help" || arg == L"-h" || arg == L"/?")
		{
			ShowUsage();
			ok = false;
		}
		else
		{
			MessageBoxW(NULL, (L"Unknown option: " + arg + L"\n\nUse --help for the list of options.").c_str(), L"GTA:Orange Launcher", MB_OK | MB_ICONERROR);
			ok = false;
		}
	}
	LocalFree(argv);
	return ok;
}

// Folder that contains Launcher.exe and orange-core.dll
static std::wstring GetLauncherDir()
{
	wchar_t path[MAX_PATH] = { 0 };
	GetModuleFileNameW(NULL, path, MAX_PATH);
	std::wstring full(path);
	size_t pos = full.find_last_of(L"\\/");
	return pos == std::wstring::npos ? full : full.substr(0, pos);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    if (!ParseCommandLine(g_options))
        return 0;
    MyRegisterClass(hInstance);

    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LAUNCHER));

    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
	Gdiplus::GdiplusShutdown(m_gdiplusToken);
    return (int) msg.wParam;
}


static void Fail(const wchar_t* message)
{
	MessageBoxW(NULL, message, L"GTA:Orange Launcher", MB_OK | MB_ICONERROR);
	TerminateProcess(GetCurrentProcess(), 1);
}

void LaunchGame()
{
	try
	{
		std::wstring orangeDir = GetLauncherDir();
		SetCurrentDirectoryW(orangeDir.c_str());

		Registry::CreateRegKeyStructure(HKEY_CURRENT_USER, L"SOFTWARE\\GTA Orange Team\\GTA Orange");
		Registry::Set_StringRegistryValue(HKEY_CURRENT_USER, L"SOFTWARE\\GTA Orange Team\\GTA Orange", L"OrangeFolder", orangeDir.c_str());

		std::string curPath = Utils::UnicodeToMultibyte(orangeDir);
		Injector::Get().PushLibrary(curPath + "\\orange-core.dll");

		// Make our folder visible to a game started by us (orange-core.dll's dependencies).
		{
			wchar_t oldPath[8192] = { 0 };
			GetEnvironmentVariableW(L"PATH", oldPath, 8192);
			std::wstring newPath = orangeDir + L";" + oldPath;
			SetEnvironmentVariableW(L"PATH", newPath.c_str());
		}

		bool isSteam = false;
		bool isPirate = false;

		UpdateSplash(0.33f);

		if (!g_options.injectOnly)
		{
			std::wstring gameFolder = g_options.gameDir;
			if (gameFolder.empty())
			{
				TCHAR TgameFolder[MAX_PATH] = { 0 };
				DWORD gameLen = MAX_PATH;
				if (!Registry::Get_StringRegistryValue(HKEY_CURRENT_USER, L"SOFTWARE\\GTA Orange Team\\GTA Orange", L"GameFolder", TgameFolder, gameLen))
				{
					CFolderBrowser folderBrowser(L"Select your GTA:V folder");
					bool folderSelected = folderBrowser.Show();
					if (!folderSelected)
					{
						TerminateProcess(GetCurrentProcess(), 0);
						return;
					}
					gameFolder = folderBrowser.GetPath();
					Registry::Set_StringRegistryValue(HKEY_CURRENT_USER, L"SOFTWARE\\GTA Orange Team\\GTA Orange", L"GameFolder", gameFolder.c_str());
				}
				else
					gameFolder = TgameFolder;
			}

			std::wstring gamePath = gameFolder + L"\\GTA5.exe";
			if (!Utils::FileExist(gamePath))
				Fail((L"GTA5.exe not found in\n" + gameFolder).c_str());

			int fs = (int)Utils::FileSize(gamePath);
			if (fs == PIRATE_EXECUTABLE_SIZE)
				isPirate = true;
			if (Utils::FileExist(gameFolder + L"\\steam_api64.dll"))
				isSteam = true;
			if (g_options.forceSteam)
				isSteam = true;
			if (g_options.forceDirect)
				isSteam = false;

			if (!isSteam || isPirate)
				Injector::Get().Run(gameFolder, gamePath);
			else
				Injector::Get().RunSteam();
		}

		UpdateSplash(0.70f);
		if (!Injector::Get().WaitUntilGameStarts(g_options.timeoutSeconds))
			Fail(L"Timed out waiting for GTA5.exe to start");
		if (!Injector::Get().InjectAll(g_options.waitForUnpack && !isPirate))
			TerminateProcess(GetCurrentProcess(), 1);
		UpdateSplash(1.0f);
	}
	catch (const std::exception& e)
	{
		MessageBoxA(NULL, e.what(), "GTA:Orange Launcher", MB_OK | MB_ICONERROR);
		TerminateProcess(GetCurrentProcess(), 1);
	}
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LAUNCHER));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_LAUNCHER));
	return RegisterClassEx(&wcex);
}


BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
	pBitmap = Bitmap::FromResource(hInstance, MAKEINTRESOURCEW(IDB_BITMAP1));

	hInst = hInstance;

	RECT rect;
	GetClientRect(GetDesktopWindow(), &rect);
	rect.left = (rect.right / 2) - (300 / 2);
	rect.top = (rect.bottom / 2) - (300 / 2);
	splashHwnd = CreateWindowW(szWindowClass, L"", WS_OVERLAPPEDWINDOW,
		rect.left, rect.top, 300, 300, nullptr, nullptr, hInstance, nullptr);
	SetWindowLong(splashHwnd, GWL_STYLE, 0);

	if (!splashHwnd)
	{
		return FALSE;
	}

	ShowWindow(splashHwnd, nCmdShow);
	UpdateWindow(splashHwnd);
	std::thread thr(LaunchGame);
	thr.detach();

	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		SwapBuffers(hdc);
		Graphics g(hdc);
		g.DrawImage(pBitmap, 0, 0);
		SolidBrush backPen(Gdiplus::Color(50, 231, 231, 231));
		SolidBrush frontPen(Gdiplus::Color(255, 130, 0));
		g.FillRectangle(&backPen, 67, 220, 168, 6);
		g.FillRectangle(&frontPen, 67, 220, (int)round(168 * loadProgress), 6);
		SwapBuffers(hdc);
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		//PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
