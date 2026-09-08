#include "stdafx.h"
#include "LauncherLog.h"

#include <ctime>
#include <mutex>

#ifndef ORANGE_VERSION
#define ORANGE_VERSION "dev"
#endif

static std::wstring g_logPath;
static std::mutex g_logMutex;

void LauncherLogInit(const std::wstring& installDir)
{
	g_logPath = installDir + L"\\launcher.log";
	LauncherLog("---- GTA:Orange Launcher " ORANGE_VERSION " started ----");
}

void LauncherLog(const std::string& line)
{
	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logPath.empty())
		return;
	std::ofstream out(g_logPath, std::ios::app);
	if (!out)
		return;
	time_t now = time(NULL);
	tm t;
	localtime_s(&t, &now);
	char stamp[32];
	strftime(stamp, sizeof(stamp), "[%Y-%m-%d %H:%M:%S] ", &t);
	out << stamp << line << std::endl;
}

void LauncherLog(const std::wstring& line)
{
	int len = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, NULL, 0, NULL, NULL);
	std::string narrow(len > 0 ? len - 1 : 0, '\0');
	if (len > 1)
		WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, &narrow[0], len, NULL, NULL);
	LauncherLog(narrow);
}
