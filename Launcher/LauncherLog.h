#pragma once
#include <string>

// Appends a timestamped line to launcher.log next to OrangeLauncher.exe.
void LauncherLogInit(const std::wstring& installDir);
void LauncherLog(const std::string& line);
void LauncherLog(const std::wstring& line);
