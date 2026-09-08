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

// File offset of an RVA inside the executable on disk, 0 when it cannot be
// determined (used to compare the code section on disk with memory).
// Reads `size` bytes at `offset` of a file that is in use by a running
// process (the game keeps its own executable open), hence the share flags.
static bool ReadFileAt(const std::wstring& path, uint64_t offset, void* out, DWORD size, DWORD* errorOut = NULL)
{
	HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		if (errorOut) *errorOut = GetLastError();
		return false;
	}
	LARGE_INTEGER position;
	position.QuadPart = (LONGLONG)offset;
	DWORD read = 0;
	bool ok = SetFilePointerEx(file, position, NULL, FILE_BEGIN) && ReadFile(file, out, size, &read, NULL) && read == size;
	if (!ok && errorOut)
		*errorOut = GetLastError();
	CloseHandle(file);
	return ok;
}

// File offset of an RVA inside the executable on disk, 0 when it cannot be
// determined (used to compare the code section on disk with memory).
static uint64_t RvaToFileOffset(const std::wstring& exePath, uint32_t rva, DWORD* errorOut = NULL)
{
	IMAGE_DOS_HEADER dos;
	if (!ReadFileAt(exePath, 0, &dos, sizeof(dos), errorOut) || dos.e_magic != IMAGE_DOS_SIGNATURE)
		return 0;
	IMAGE_NT_HEADERS64 nt;
	if (!ReadFileAt(exePath, (uint64_t)dos.e_lfanew, &nt, sizeof(nt), errorOut) || nt.Signature != IMAGE_NT_SIGNATURE)
		return 0;
	uint64_t sections = (uint64_t)dos.e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
	for (int i = 0; i < nt.FileHeader.NumberOfSections; ++i)
	{
		IMAGE_SECTION_HEADER section;
		if (!ReadFileAt(exePath, sections + (uint64_t)i * sizeof(section), &section, sizeof(section), errorOut))
			return 0;
		uint32_t size = section.Misc.VirtualSize > section.SizeOfRawData ? section.Misc.VirtualSize : section.SizeOfRawData;
		if (rva >= section.VirtualAddress && rva < section.VirtualAddress + size)
			return (uint64_t)section.PointerToRawData + (rva - section.VirtualAddress);
	}
	return 0;
}

// The retail executable is packed; wait until its code section has been
// rewritten in memory before patching anything. When the game was started
// by someone else (--inject) that usually happened already, which shows as
// memory differing from the file on disk. Gives up after the timeout.
bool Injector::WaitForUnpackFinished(int pid, int timeoutSeconds)
{
	HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (!process)
	{
		LauncherLog("unpack wait: OpenProcess failed (" + LastErrorText() + ")");
		return false;
	}

	ULONGLONG started = GetTickCount64();
	ULONGLONG deadline = started + (ULONGLONG)timeoutSeconds * 1000ULL;
	ULONGLONG nextHeartbeat = started + 15000ULL;
	HMODULE hMod = NULL;
	std::wstring exePath;
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
						exePath = ModEnt.szExePath;
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
			if (GetTickCount64() > nextHeartbeat)
			{
				LauncherLog("unpack wait: still looking for the GTA5.exe module in the process (" + std::to_string((GetTickCount64() - started) / 1000) + "s)");
				nextHeartbeat += 15000ULL;
			}
			Sleep(50);
		}
	}
	LauncherLog(L"unpack wait: GTA5.exe module found, image " + exePath);

	unsigned char buff[10] = { 0 };
	if (!ReadProcessMemory(process, (LPVOID)((uint64_t)hMod + 0x1000), buff, 10, NULL))
	{
		LauncherLog("unpack wait: ReadProcessMemory failed (" + LastErrorText() + ")");
		CloseHandle(process);
		return false;
	}

	unsigned char disk[10] = { 0 };
	DWORD diskError = 0;
	uint64_t fileOffset = exePath.empty() ? 0 : RvaToFileOffset(exePath, 0x1000, &diskError);
	if (fileOffset && ReadFileAt(exePath, fileOffset, disk, sizeof(disk), &diskError))
	{
		if (memcmp(buff, disk, sizeof(disk)) != 0)
		{
			LauncherLog("unpack wait: the code section in memory already differs from the file on disk, executable is unpacked");
			CloseHandle(process);
			return true;
		}
		LauncherLog("unpack wait: code section still equals the file on disk, waiting for it to change");
	}
	else
		LauncherLog("unpack wait: cannot compare with the file on disk (error " + std::to_string(diskError)
			+ "), waiting for the code section to change in memory");

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
		if (GetTickCount64() > nextHeartbeat)
		{
			LauncherLog("unpack wait: still waiting for the code section to change (" + std::to_string((GetTickCount64() - started) / 1000) + "s)");
			nextHeartbeat += 15000ULL;
		}
	}
}


// "1.0.3889.0" from the version resource of the executable on disk, or "" when
// it cannot be read.
static std::wstring FileVersionOf(const std::wstring& path)
{
	DWORD handle = 0;
	DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
	if (!size)
		return L"";
	std::vector<char> buffer(size);
	if (!GetFileVersionInfoW(path.c_str(), 0, size, buffer.data()))
		return L"";
	VS_FIXEDFILEINFO* info = NULL;
	UINT length = 0;
	if (!VerQueryValueW(buffer.data(), L"\\", (LPVOID*)&info, &length) || !info || length < sizeof(VS_FIXEDFILEINFO))
		return L"";
	wchar_t text[64];
	swprintf_s(text, L"%u.%u.%u.%u", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
		HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
	return text;
}

// The retail executable is protected on disk; the code only exists in clear
// inside the running process. This copies the whole image page by page
// (unreadable pages are zeroed) and rewrites the section table so that the
// file offsets equal the RVAs, which is what disassemblers expect from a
// memory dump. Nothing is written back to the game.
bool Injector::DumpGame(const std::wstring& outputDir, std::wstring& writtenPath, std::string& error)
{
	int pid = FindProcess(PROCESS_NAME);
	if (pid == -1)
	{
		error = "GTA5.exe is not running";
		return false;
	}

	uintptr_t base = 0;
	DWORD imageSize = 0;
	std::wstring exePath;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (snapshot != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 entry;
		entry.dwSize = sizeof(entry);
		if (Module32First(snapshot, &entry))
		{
			do {
				if (_wcsicmp(PROCESS_NAME, entry.szModule) == 0)
				{
					base = (uintptr_t)entry.modBaseAddr;
					imageSize = entry.modBaseSize;
					exePath = entry.szExePath;
					break;
				}
			} while (Module32Next(snapshot, &entry));
		}
		CloseHandle(snapshot);
	}
	if (!base || !imageSize)
	{
		error = "GTA5.exe module not found in process " + std::to_string(pid);
		return false;
	}
	LauncherLog("dump: GTA5.exe pid " + std::to_string(pid) + ", image base 0x" + std::to_string((unsigned long long)base)
		+ " (decimal), image size " + std::to_string(imageSize) + " bytes");

	HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (!process)
	{
		error = "OpenProcess failed (" + LastErrorText() + ")";
		return false;
	}
	std::vector<unsigned char> image(imageSize);
	const SIZE_T page = 0x1000;
	SIZE_T unreadable = 0;
	for (SIZE_T offset = 0; offset < imageSize; offset += page)
	{
		SIZE_T chunk = (imageSize - offset) < page ? (imageSize - offset) : page;
		SIZE_T read = 0;
		if (!ReadProcessMemory(process, (LPCVOID)(base + offset), &image[offset], chunk, &read) || read != chunk)
		{
			memset(&image[offset], 0, chunk);
			unreadable += chunk;
		}
	}
	CloseHandle(process);

	if (imageSize < sizeof(IMAGE_DOS_HEADER))
	{
		error = "image too small";
		return false;
	}
	IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)image.data();
	if (dos->e_magic != IMAGE_DOS_SIGNATURE || (DWORD)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > imageSize)
	{
		error = "the dumped image has no valid PE header (was the first page readable?)";
		return false;
	}
	IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(image.data() + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
	{
		error = "the dumped image has no valid PE header";
		return false;
	}
	DWORD align = nt->OptionalHeader.SectionAlignment ? nt->OptionalHeader.SectionAlignment : 0x1000;
	IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
	for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
	{
		DWORD size = sections[i].Misc.VirtualSize ? sections[i].Misc.VirtualSize : sections[i].SizeOfRawData;
		sections[i].PointerToRawData = sections[i].VirtualAddress;
		sections[i].SizeOfRawData = (size + align - 1) / align * align;
	}
	nt->OptionalHeader.FileAlignment = align;

	std::wstring version = exePath.empty() ? L"" : FileVersionOf(exePath);
	if (version.empty())
		version = L"unknown-version";
	writtenPath = outputDir + L"\\GTA5-" + version + L".dump.exe";
	std::ofstream out(writtenPath, std::ios::binary | std::ios::trunc);
	if (!out)
	{
		error = "cannot write " + Utils::UnicodeToMultibyte(writtenPath);
		return false;
	}
	out.write((const char*)image.data(), (std::streamsize)image.size());
	if (!out.good())
	{
		error = "writing " + Utils::UnicodeToMultibyte(writtenPath) + " failed (disk full?)";
		return false;
	}
	LauncherLog(L"dump: written " + writtenPath);
	LauncherLog("dump: game version " + Utils::UnicodeToMultibyte(version) + ", " + std::to_string(unreadable)
		+ " unreadable bytes zeroed; the file offsets equal the RVAs, so an address in the dump minus 0 is the RVA for offsets.ini");
	return true;
}

Injector::~Injector()
{
}

std::wstring Injector::GameFileVersion(const std::wstring& exePath)
{
	return FileVersionOf(exePath);
}

// The same version, read from the running process instead of from the file.
// Under Proton the game's own path ("S:\\steamapps\\...") is a drive mapping the
// launcher process often cannot open (CreateFileW fails with ERROR_PATH_NOT_FOUND),
// so the file based read above returns nothing there. The mapped image always
// carries the version resource, and reading process memory needs no file access.
std::wstring Injector::GameProcessVersion()
{
	int processId = Get().FindProcess(PROCESS_NAME);
	if (processId == -1)
		return L"";
	HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
	if (!process)
		return L"";

	uintptr_t base = 0;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
	if (snapshot != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 module;
		module.dwSize = sizeof(module);
		if (Module32First(snapshot, &module))
		{
			do {
				if (_wcsicmp(PROCESS_NAME, module.szModule) == 0)
				{
					base = (uintptr_t)module.modBaseAddr;
					break;
				}
			} while (Module32Next(snapshot, &module));
		}
		CloseHandle(snapshot);
	}
	if (!base)
	{
		CloseHandle(process);
		return L"";
	}

	std::wstring version;
	IMAGE_DOS_HEADER dos = { 0 };
	IMAGE_NT_HEADERS64 nt = { 0 };
	SIZE_T read = 0;
	if (ReadProcessMemory(process, (LPCVOID)base, &dos, sizeof(dos), &read) && read == sizeof(dos)
		&& dos.e_magic == IMAGE_DOS_SIGNATURE
		&& ReadProcessMemory(process, (LPCVOID)(base + dos.e_lfanew), &nt, sizeof(nt), &read) && read == sizeof(nt)
		&& nt.Signature == IMAGE_NT_SIGNATURE)
	{
		const IMAGE_DATA_DIRECTORY& resources = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
		// The whole resource directory of GTA5.exe is well under a megabyte;
		// the cap only keeps a corrupt header from asking for a huge buffer.
		const DWORD kMaxResourceBytes = 8 * 1024 * 1024;
		if (resources.VirtualAddress && resources.Size && resources.Size <= kMaxResourceBytes)
		{
			std::vector<unsigned char> buffer(resources.Size);
			if (ReadProcessMemory(process, (LPCVOID)(base + resources.VirtualAddress), buffer.data(), buffer.size(), &read) && read >= 16)
			{
				// VS_FIXEDFILEINFO: dwSignature 0xFEEF04BD, then dwStrucVersion,
				// dwFileVersionMS, dwFileVersionLS.
				const unsigned char signature[4] = { 0xBD, 0x04, 0xEF, 0xFE };
				for (size_t i = 0; i + 16 <= read; ++i)
				{
					if (memcmp(buffer.data() + i, signature, sizeof(signature)) != 0)
						continue;
					DWORD ms = 0, ls = 0;
					memcpy(&ms, buffer.data() + i + 8, sizeof(ms));
					memcpy(&ls, buffer.data() + i + 12, sizeof(ls));
					wchar_t text[64];
					swprintf_s(text, L"%u.%u.%u.%u", HIWORD(ms), LOWORD(ms), HIWORD(ls), LOWORD(ls));
					version = text;
					break;
				}
			}
		}
	}
	CloseHandle(process);
	return version;
}

std::wstring Injector::FindGameExePath()
{
	int pid = FindProcess(PROCESS_NAME);
	if (pid == -1)
		return L"";
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!process)
		return L"";
	wchar_t path[MAX_PATH * 2] = { 0 };
	DWORD size = MAX_PATH * 2;
	std::wstring result;
	if (QueryFullProcessImageNameW(process, 0, path, &size))
		result = path;
	CloseHandle(process);
	return result;
}
