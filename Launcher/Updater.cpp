#include "stdafx.h"
#include "Updater.h"
#include "UpdateManifest.h"
#include "LauncherLog.h"

#include <winhttp.h>
#include <bcrypt.h>
#include <iomanip>
#include <iterator>
#include <sstream>

#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 0x00000800
#endif
#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3 0x00002000
#endif

namespace
{
	const size_t kMaxDownloadSize = 256u * 1024u * 1024u;   // sanity limit per file

	std::wstring Widen(const std::string& s)
	{
		if (s.empty())
			return std::wstring();
		int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
		std::wstring out(len, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
		return out;
	}

	std::string Narrow(const std::wstring& s)
	{
		if (s.empty())
			return std::string();
		int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0, NULL, NULL);
		std::string out(len, '\0');
		WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len, NULL, NULL);
		return out;
	}

	std::string ErrorText(const char* what, DWORD error)
	{
		std::ostringstream ss;
		ss << what << " (error " << error << ")";
		return ss.str();
	}

	struct InternetHandle
	{
		HINTERNET handle;
		explicit InternetHandle(HINTERNET h) : handle(h) {}
		~InternetHandle() { if (handle) WinHttpCloseHandle(handle); }
		operator HINTERNET() const { return handle; }
	};

	bool Sha256Hex(const std::vector<char>& data, std::string& hex)
	{
		BCRYPT_ALG_HANDLE algorithm = NULL;
		BCRYPT_HASH_HANDLE hash = NULL;
		if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0)
			return false;

		DWORD objectLength = 0, resultLength = 0;
		bool ok = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objectLength, sizeof(objectLength), &resultLength, 0) >= 0;
		std::vector<UCHAR> object(ok ? objectLength : 0);
		if (ok)
			ok = BCryptCreateHash(algorithm, &hash, object.data(), objectLength, NULL, 0, 0) >= 0;
		if (ok && !data.empty())
			ok = BCryptHashData(hash, (PUCHAR)data.data(), (ULONG)data.size(), 0) >= 0;
		UCHAR digest[32] = { 0 };
		if (ok)
			ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
		if (hash)
			BCryptDestroyHash(hash);
		BCryptCloseAlgorithmProvider(algorithm, 0);
		if (!ok)
			return false;

		std::ostringstream ss;
		ss << std::hex << std::setfill('0');
		for (int i = 0; i < 32; ++i)
			ss << std::setw(2) << (unsigned)digest[i];
		hex = ss.str();
		return true;
	}

	bool ReadWholeFile(const std::wstring& path, std::vector<char>& data)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in)
			return false;
		data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		return true;
	}

	std::wstring OwnExecutableName()
	{
		wchar_t path[MAX_PATH] = { 0 };
		GetModuleFileNameW(NULL, path, MAX_PATH);
		std::wstring full(path);
		size_t pos = full.find_last_of(L"\\/");
		return pos == std::wstring::npos ? full : full.substr(pos + 1);
	}
}

Updater::Updater(const std::wstring& installDir, const std::string& localVersion, const UpdaterSettings& settings, ProgressFn progress)
	: m_installDir(installDir), m_localVersion(localVersion), m_settings(settings), m_progress(progress)
{
}

void Updater::Progress(const std::wstring& status, float progress)
{
	if (m_progress)
		m_progress(status, progress);
}

std::wstring Updater::BaseUrl() const
{
	std::wstring base = L"https://github.com/" + m_settings.repository;
	if (m_settings.channel == L"nightly")
		return base + L"/releases/download/nightly";
	return base + L"/releases/latest/download";
}

bool Updater::HttpGet(const std::wstring& url, std::vector<char>& out, const std::wstring& what, float progressFrom, float progressTo)
{
	out.clear();

	wchar_t host[256] = { 0 };
	wchar_t path[2048] = { 0 };
	URL_COMPONENTS components;
	ZeroMemory(&components, sizeof(components));
	components.dwStructSize = sizeof(components);
	components.lpszHostName = host;
	components.dwHostNameLength = 256;
	components.lpszUrlPath = path;
	components.dwUrlPathLength = 2048;
	if (!WinHttpCrackUrl(url.c_str(), 0, 0, &components))
	{
		m_error = "invalid URL " + Narrow(url);
		return false;
	}

	std::wstring userAgent = L"GTA-Orange-Launcher/" + Widen(m_localVersion);
	InternetHandle session(WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
	if (!session)
	{
		m_error = ErrorText("WinHttpOpen failed", GetLastError());
		return false;
	}
	WinHttpSetTimeouts(session, 15000, 15000, 30000, 60000);
	DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
	if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols)))
	{
		protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
		WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
	}

	InternetHandle connection(WinHttpConnect(session, host, components.nPort, 0));
	if (!connection)
	{
		m_error = ErrorText("WinHttpConnect failed", GetLastError());
		return false;
	}

	DWORD flags = (components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
	InternetHandle request(WinHttpOpenRequest(connection, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
	if (!request)
	{
		m_error = ErrorText("WinHttpOpenRequest failed", GetLastError());
		return false;
	}

	// GitHub answers with redirects (releases/latest -> release -> CDN); WinHTTP follows them.
	if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
		!WinHttpReceiveResponse(request, NULL))
	{
		m_error = ErrorText("request failed", GetLastError());
		return false;
	}

	DWORD status = 0;
	DWORD size = sizeof(status);
	WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
	if (status != 200)
	{
		m_error = "HTTP " + std::to_string(status) + " for " + Narrow(url);
		return false;
	}

	DWORD contentLength = 0;
	size = sizeof(contentLength);
	if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &size, WINHTTP_NO_HEADER_INDEX))
		contentLength = 0;

	for (;;)
	{
		DWORD available = 0;
		if (!WinHttpQueryDataAvailable(request, &available))
		{
			m_error = ErrorText("WinHttpQueryDataAvailable failed", GetLastError());
			return false;
		}
		if (available == 0)
			break;
		size_t offset = out.size();
		if (offset + available > kMaxDownloadSize)
		{
			m_error = "download too large: " + Narrow(url);
			return false;
		}
		out.resize(offset + available);
		DWORD read = 0;
		if (!WinHttpReadData(request, &out[offset], available, &read))
		{
			m_error = ErrorText("WinHttpReadData failed", GetLastError());
			return false;
		}
		out.resize(offset + read);
		if (read == 0)
			break;
		if (contentLength > 0)
			Progress(what, progressFrom + (progressTo - progressFrom) * (float)out.size() / (float)contentLength);
	}
	return true;
}

bool Updater::HashFile(const std::wstring& path, std::string& sha256Hex)
{
	std::vector<char> data;
	if (!ReadWholeFile(path, data))
		return false;
	return Sha256Hex(data, sha256Hex);
}

bool Updater::WriteWholeFile(const std::wstring& path, const std::vector<char>& data)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out)
		return false;
	if (!data.empty())
		out.write(data.data(), (std::streamsize)data.size());
	return out.good();
}

UpdateResult Updater::Run()
{
	m_error.clear();
	if (m_settings.channel.empty())
		m_settings.channel = Widen(DefaultChannelFor(m_localVersion));
	if (!m_settings.enabled)
	{
		LauncherLog("updater: disabled in launcher.xml / --no-update");
		return UpdateResult::Skipped;
	}
	if (IsDevVersion(m_localVersion) && !m_settings.force)
	{
		LauncherLog("updater: development build (" + m_localVersion + "), not updating (use --update to force)");
		return UpdateResult::Skipped;
	}

	std::wstring base = BaseUrl();
	LauncherLog(L"updater: channel " + m_settings.channel + L", manifest " + base + L"/client-manifest.txt");
	Progress(L"Checking for updates...", 0.02f);

	std::vector<char> body;
	if (!HttpGet(base + L"/client-manifest.txt", body, L"Checking for updates...", 0.02f, 0.05f))
		return UpdateResult::Failed;

	UpdateManifest manifest;
	if (!UpdateManifest::Parse(std::string(body.begin(), body.end()), manifest))
	{
		m_error = "the update manifest is invalid";
		return UpdateResult::Failed;
	}
	m_remoteVersion = manifest.version;
	LauncherLog("updater: local version " + m_localVersion + ", remote version " + manifest.version);

	// Never move between channels on our own (a nightly client on the stable
	// channel would be downgraded to the older release).
	if (IsNightlyVersion(m_localVersion) != IsNightlyVersion(manifest.version) && !m_settings.force && !m_settings.channelExplicit)
	{
		LauncherLog("updater: " + manifest.version + " belongs to a different channel than this build (" + m_localVersion
			+ "), not switching automatically; run OrangeLauncher.exe --channel " + Narrow(m_settings.channel) + " --update to switch");
		return UpdateResult::Skipped;
	}

	std::vector<const UpdateFile*> outdated;
	for (const UpdateFile& file : manifest.files)
	{
		std::string localHash;
		if (!HashFile(m_installDir + L"\\" + Widen(file.name), localHash) || localHash != file.sha256)
			outdated.push_back(&file);
	}
	if (outdated.empty())
	{
		LauncherLog("updater: up to date");
		return UpdateResult::UpToDate;
	}

	// 1) download everything to <name>.new and verify it
	float progress = 0.05f;
	float step = 0.25f / (float)outdated.size();
	for (const UpdateFile* file : outdated)
	{
		std::wstring name = Widen(file->name);
		LauncherLog(L"updater: downloading " + name);
		std::vector<char> data;
		if (!HttpGet(base + L"/" + name, data, L"Downloading " + name + L"...", progress, progress + step))
			return UpdateResult::Failed;
		progress += step;

		std::string hash;
		if (!Sha256Hex(data, hash) || hash != file->sha256)
		{
			m_error = "checksum mismatch for " + file->name;
			return UpdateResult::Failed;
		}
		if (!WriteWholeFile(m_installDir + L"\\" + name + L".new", data))
		{
			m_error = "cannot write " + file->name + ".new (is the folder writable?)";
			return UpdateResult::Failed;
		}
	}

	// 2) put the new files in place
	bool restart = false;
	std::wstring self = OwnExecutableName();
	for (const UpdateFile* file : outdated)
	{
		std::wstring name = Widen(file->name);
		std::wstring target = m_installDir + L"\\" + name;
		std::wstring fresh = target + L".new";
		if (_wcsicmp(name.c_str(), self.c_str()) == 0)
		{
			// A running executable cannot be overwritten, but it can be renamed.
			std::wstring old = target + L".old";
			DeleteFileW(old.c_str());
			if (!MoveFileExW(target.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING))
			{
				m_error = ErrorText("cannot rename the running launcher", GetLastError());
				return UpdateResult::Failed;
			}
			if (!MoveFileExW(fresh.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING))
			{
				DWORD error = GetLastError();
				MoveFileExW(old.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);   // roll back
				m_error = ErrorText("cannot install the new launcher", error);
				return UpdateResult::Failed;
			}
			restart = true;
		}
		else if (!MoveFileExW(fresh.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING))
		{
			m_error = ErrorText(("cannot replace " + file->name).c_str(), GetLastError());
			return UpdateResult::Failed;
		}
		LauncherLog(L"updater: installed " + name);
	}

	LauncherLog("updater: updated to version " + manifest.version + (restart ? " (restart required)" : ""));
	Progress(L"Update installed", 0.3f);
	return restart ? UpdateResult::RestartRequired : UpdateResult::Updated;
}

void Updater::CleanupAfterRestart(const std::wstring& installDir)
{
	std::wstring old = installDir + L"\\" + OwnExecutableName() + L".old";
	for (int attempt = 0; attempt < 20; ++attempt)
	{
		if (DeleteFileW(old.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND)
			return;
		Sleep(100);   // the previous launcher instance may still be exiting
	}
}

#ifndef ORANGE_VERSION
#define ORANGE_VERSION "dev"
#endif

bool Updater::DownloadUrl(const std::wstring& url, std::vector<char>& out, std::string& error)
{
	UpdaterSettings settings;
	Updater downloader(L"", ORANGE_VERSION, settings, ProgressFn());
	if (downloader.HttpGet(url, out, L"Downloading...", 0.f, 0.f))
		return true;
	error = downloader.m_error;
	return false;
}
