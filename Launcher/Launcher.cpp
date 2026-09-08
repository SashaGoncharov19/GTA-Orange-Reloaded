// Launcher.cpp: определяет точку входа для приложения.
//

#include "stdafx.h"
#include "Launcher.h"
#include "Injector.h"
#include "Updater.h"
#include "LauncherLog.h"
#include <thread>
#include <mutex>

#define MAX_LOADSTRING 100
#ifndef ORANGE_VERSION
#define ORANGE_VERSION "dev"
#endif
#define ORANGE_WIDEN2(x) L##x
#define ORANGE_WIDEN(x) ORANGE_WIDEN2(x)
#define ORANGE_VERSION_W ORANGE_WIDEN(ORANGE_VERSION)
#define PIRATE_EXECUTABLE_SIZE 54528512

// Глобальные переменные:
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING] = L"GTA:Orange Launcher";
WCHAR szWindowClass[MAX_LOADSTRING] = L"_gtaorange_launcher";
Image* pBitmap = NULL;
float loadProgress = 0.0;
static std::wstring g_splashStatus;
static std::mutex g_statusMutex;
HWND splashHwnd = NULL;
ULONG_PTR m_gdiplusToken;


ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void				UpdateSplash(float progress);

// Text shown below the progress bar of the splash screen (thread safe).
void SetSplashStatus(const std::wstring& status)
{
	{
		std::lock_guard<std::mutex> lock(g_statusMutex);
		g_splashStatus = status;
	}
	if (splashHwnd)
		InvalidateRect(splashHwnd, NULL, FALSE);
}

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
	bool checkUpdates = true;     // --no-update
	bool forceUpdate = false;     // --update (also updates -dev builds)
	bool afterUpdate = false;     // --updated (internal: just restarted after a self-update)
	std::wstring channel;         // --channel stable|nightly (overrides launcher.xml)
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
		L"  --no-update         skip the update check\n"
		L"  --update            force an update check (even for development builds)\n"
		L"  --channel <name>    update channel: stable (default) or nightly\n"
		L"  --version           show the launcher version\n"
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
		else if (arg == L"--no-update")
			options.checkUpdates = false;
		else if (arg == L"--update")
			options.forceUpdate = true;
		else if (arg == L"--updated")
			options.afterUpdate = true;
		else if (arg == L"--channel" && i + 1 < argc)
			options.channel = argv[++i];
		else if (arg == L"--version" || arg == L"-v")
		{
			MessageBoxW(NULL, L"GTA:Orange Launcher " ORANGE_VERSION_W, L"GTA:Orange Launcher", MB_OK | MB_ICONINFORMATION);
			ok = false;
		}
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

static std::wstring FromUtf8(const char* text)
{
	if (!text || !*text)
		return std::wstring();
	int len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
	std::wstring out(len > 0 ? len - 1 : 0, L'\0');
	if (len > 1)
		MultiByteToWideChar(CP_UTF8, 0, text, -1, &out[0], len);
	return out;
}

// launcher.xml next to Launcher.exe:
//   <launcher><updates enabled="true" channel="stable" repository="owner/repo"/></launcher>
static void LoadLauncherSettings(const std::wstring& dir, UpdaterSettings& settings)
{
	std::ifstream in(dir + L"\\launcher.xml", std::ios::binary);
	if (!in)
		return;
	std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	tinyxml2::XMLDocument doc;
	if (doc.Parse(content.c_str(), content.size()) != tinyxml2::XML_SUCCESS)
	{
		LauncherLog("launcher.xml could not be parsed, using defaults");
		return;
	}
	tinyxml2::XMLElement* root = doc.FirstChildElement("launcher");
	tinyxml2::XMLElement* updates = root ? root->FirstChildElement("updates") : NULL;
	if (!updates)
		return;
	updates->QueryBoolAttribute("enabled", &settings.enabled);
	if (const char* channel = updates->Attribute("channel"))
		settings.channel = FromUtf8(channel);
	if (const char* repository = updates->Attribute("repository"))
		settings.repository = FromUtf8(repository);
}

// Starts a fresh copy of (the just updated) Launcher.exe with the same
// arguments and exits.
static void RestartLauncher()
{
	wchar_t exe[MAX_PATH] = { 0 };
	GetModuleFileNameW(NULL, exe, MAX_PATH);
	std::wstring commandLine = GetCommandLineW();
	commandLine += L" --updated";
	std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
	buffer.push_back(L'\0');

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	if (CreateProcessW(exe, buffer.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		LauncherLog("restarting after self-update");
	}
	else
	{
		LauncherLog("restart after self-update failed");
		MessageBoxW(NULL, L"The launcher was updated. Please start it again.", L"GTA:Orange Launcher", MB_OK | MB_ICONINFORMATION);
	}
	TerminateProcess(GetCurrentProcess(), 0);
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
	LauncherLog(std::wstring(L"FATAL: ") + message);
	MessageBoxW(NULL, message, L"GTA:Orange Launcher", MB_OK | MB_ICONERROR);
	TerminateProcess(GetCurrentProcess(), 1);
}

static std::string BoolText(bool value)
{
	return value ? "yes" : "no";
}

void LaunchGame()
{
	try
	{
		std::wstring orangeDir = GetLauncherDir();
		SetCurrentDirectoryW(orangeDir.c_str());
		LauncherLogInit(orangeDir);
		LauncherLog("---- Launcher " ORANGE_VERSION " starting ----");
		LauncherLog(L"launcher folder: " + orangeDir);
		LauncherLog(L"command line: " + std::wstring(GetCommandLineW()));
		LauncherLog("mode: " + std::string(g_options.injectOnly ? "inject into a running GTA5.exe (--inject)" : "start the game and inject")
			+ ", wait for unpack: " + BoolText(g_options.waitForUnpack)
			+ ", timeout: " + std::to_string(g_options.timeoutSeconds) + "s"
			+ ", update check: " + BoolText(g_options.checkUpdates && !g_options.afterUpdate));
		Updater::CleanupAfterRestart(orangeDir);

		// Auto-update (never blocks the game start: failures are logged only).
		if (g_options.checkUpdates && !g_options.afterUpdate)
		{
			UpdaterSettings settings;
			LoadLauncherSettings(orangeDir, settings);
			if (!g_options.channel.empty())
				settings.channel = g_options.channel;
			if (g_options.forceUpdate)
			{
				settings.enabled = true;
				settings.force = true;
			}
			Updater updater(orangeDir, ORANGE_VERSION, settings, [](const std::wstring& status, float progress)
			{
				SetSplashStatus(status);
				UpdateSplash(progress);
			});
			LauncherLog(L"update check: channel " + settings.channel + L", repository " + settings.repository
				+ (settings.enabled ? L"" : L" (disabled in launcher.xml)"));
			UpdateResult result = updater.Run();
			if (result == UpdateResult::RestartRequired)
			{
				LauncherLog("updater: launcher updated, restart required");
				SetSplashStatus(L"Restarting...");
				RestartLauncher();
				return;
			}
			if (result == UpdateResult::Failed)
			{
				LauncherLog("updater: FAILED: " + updater.LastError() + " (starting the game anyway)");
				SetSplashStatus(L"Update check failed, starting anyway");
				Sleep(1500);
			}
			else
				LauncherLog(std::string("updater: ") + (result == UpdateResult::Updated ? "client files updated" :
					result == UpdateResult::UpToDate ? "client is up to date" : "skipped"));
		}
		else
			LauncherLog(g_options.afterUpdate ? "update check: skipped (just restarted after a self-update)" : "update check: skipped (--no-update)");
		SetSplashStatus(g_options.injectOnly ? L"Waiting for GTA5.exe..." : L"Starting GTA V...");

		Registry::CreateRegKeyStructure(HKEY_CURRENT_USER, L"SOFTWARE\\GTA Orange Team\\GTA Orange");
		Registry::Set_StringRegistryValue(HKEY_CURRENT_USER, L"SOFTWARE\\GTA Orange Team\\GTA Orange", L"OrangeFolder", orangeDir.c_str());

		std::string curPath = Utils::UnicodeToMultibyte(orangeDir);
		Injector::Get().PushLibrary(curPath + "\\orange-core.dll");
		LauncherLog("library to inject: " + curPath + "\\orange-core.dll");

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
			if (!gameFolder.empty())
				LauncherLog(L"game folder from --game-dir: " + gameFolder);
			if (gameFolder.empty())
			{
				TCHAR TgameFolder[MAX_PATH] = { 0 };
				DWORD gameLen = MAX_PATH;
				if (!Registry::Get_StringRegistryValue(HKEY_CURRENT_USER, L"SOFTWARE\\GTA Orange Team\\GTA Orange", L"GameFolder", TgameFolder, gameLen))
				{
					LauncherLog("game folder not in the registry, asking the user");
					CFolderBrowser folderBrowser(L"Select your GTA:V folder");
					bool folderSelected = folderBrowser.Show();
					if (!folderSelected)
					{
						LauncherLog("folder dialog cancelled, exiting");
						TerminateProcess(GetCurrentProcess(), 0);
						return;
					}
					gameFolder = folderBrowser.GetPath();
					Registry::Set_StringRegistryValue(HKEY_CURRENT_USER, L"SOFTWARE\\GTA Orange Team\\GTA Orange", L"GameFolder", gameFolder.c_str());
					LauncherLog(L"game folder selected: " + gameFolder);
				}
				else
				{
					gameFolder = TgameFolder;
					LauncherLog(L"game folder from the registry: " + gameFolder);
				}
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
			LauncherLog("GTA5.exe size: " + std::to_string(fs) + " bytes, steam: " + BoolText(isSteam)
				+ ", known non-retail executable: " + BoolText(isPirate));

			if (!isSteam || isPirate)
			{
				LauncherLog(L"starting GTA5.exe directly: " + gamePath);
				Injector::Get().Run(gameFolder, gamePath);
			}
			else
			{
				LauncherLog("starting the game through Steam (steam://run/271590)");
				Injector::Get().RunSteam();
			}
		}

		UpdateSplash(0.70f);
		LauncherLog("waiting for GTA5.exe (up to " + std::to_string(g_options.timeoutSeconds) + "s)");
		if (!Injector::Get().WaitUntilGameStarts(g_options.timeoutSeconds))
			Fail(L"Timed out waiting for GTA5.exe to start");
		SetSplashStatus(L"Injecting orange-core.dll...");
		if (!Injector::Get().InjectAll(g_options.waitForUnpack && !isPirate))
		{
			LauncherLog("injection FAILED, see above; the game keeps running without GTA:Orange");
			TerminateProcess(GetCurrentProcess(), 1);
		}
		LauncherLog("done: orange-core.dll injected, see client.log for what happens inside the game");
		UpdateSplash(1.0f);
	}
	catch (const std::exception& e)
	{
		LauncherLog(std::string("FATAL: ") + e.what());
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
		{
			std::lock_guard<std::mutex> lock(g_statusMutex);
			if (!g_splashStatus.empty())
			{
				Gdiplus::Font font(L"Segoe UI", 8.5f);
				if (font.GetLastStatus() == Gdiplus::Ok)
				{
					Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 255, 255, 255));
					Gdiplus::StringFormat format;
					format.SetAlignment(Gdiplus::StringAlignmentCenter);
					Gdiplus::RectF layout(10.0f, 232.0f, 280.0f, 40.0f);
					g.DrawString(g_splashStatus.c_str(), -1, &font, layout, &format, &textBrush);
				}
			}
		}
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
