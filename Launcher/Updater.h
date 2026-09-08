#pragma once
#include <functional>
#include <string>
#include <vector>

// Auto-updater for the client package.
//
// Every GitHub release (a "v*" tag = stable channel, the rolling "nightly"
// pre-release = nightly channel) publishes client-manifest.txt plus the
// individual client files. Before the game is started the launcher downloads
// the manifest, compares SHA-256 hashes with the local files and replaces the
// ones that differ. Launcher.exe replaces itself by renaming the running
// executable to Launcher.exe.old and restarts afterwards.
//
// Settings come from launcher.xml next to Launcher.exe (see runtime/client)
// and can be overridden with --no-update / --update / --channel <name>.

struct UpdaterSettings
{
	bool enabled = true;
	bool force = false;                 // update even a "-dev" build
	std::wstring channel = L"stable";   // "stable" (latest release) or "nightly"
	std::wstring repository = L"SashaGoncharov19/GTA-Orange-Reloaded";
};

enum class UpdateResult
{
	UpToDate,
	Updated,           // files replaced, no restart needed
	RestartRequired,   // Launcher.exe itself was replaced
	Skipped,           // disabled or development build
	Failed             // network / verification problem, game start continues
};

class Updater
{
public:
	typedef std::function<void(const std::wstring& status, float progress)> ProgressFn;

	Updater(const std::wstring& installDir, const std::string& localVersion, const UpdaterSettings& settings, ProgressFn progress);

	UpdateResult Run();
	const std::string& LastError() const { return m_error; }
	const std::string& RemoteVersion() const { return m_remoteVersion; }

	// Removes Launcher.exe.old left behind by a self-update.
	static void CleanupAfterRestart(const std::wstring& installDir);

private:
	std::wstring BaseUrl() const;
	bool HttpGet(const std::wstring& url, std::vector<char>& out, const std::wstring& what, float progressFrom, float progressTo);
	bool HashFile(const std::wstring& path, std::string& sha256Hex);
	bool WriteWholeFile(const std::wstring& path, const std::vector<char>& data);
	void Progress(const std::wstring& status, float progress);

	std::wstring m_installDir;
	std::string m_localVersion;
	std::string m_remoteVersion;
	UpdaterSettings m_settings;
	ProgressFn m_progress;
	std::string m_error;
};
