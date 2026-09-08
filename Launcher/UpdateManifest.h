// Parser for the client update manifest published with every GitHub release
// (client-manifest.txt). Kept free of Windows dependencies so it can be unit
// tested on any platform (tests/manifest_test.cpp).
//
// Format (one entry per line, '#' starts a comment):
//
//   version 0.2.0
//   file orange-core.dll <sha256 hex> <size in bytes>
//   file OrangeLauncher.exe    <sha256 hex> <size in bytes>

#pragma once

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

struct UpdateFile
{
	std::string name;
	std::string sha256;                // lowercase hex, 64 characters
	unsigned long long size = 0;
};

struct UpdateManifest
{
	std::string version;
	std::vector<UpdateFile> files;

	bool IsValid() const { return !version.empty() && !files.empty(); }

	static bool Parse(const std::string& text, UpdateManifest& out)
	{
		UpdateManifest manifest;
		std::istringstream in(text);
		std::string line;
		while (std::getline(in, line))
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			size_t comment = line.find('#');
			if (comment != std::string::npos)
				line.erase(comment);

			std::istringstream ls(line);
			std::string key;
			if (!(ls >> key))
				continue;

			if (key == "version")
			{
				ls >> manifest.version;
			}
			else if (key == "file")
			{
				UpdateFile file;
				ls >> file.name >> file.sha256 >> file.size;
				if (file.name.empty() || file.sha256.size() != 64)
					return false;
				// never let a manifest write outside of the install folder
				if (file.name.find_first_of("/\\:") != std::string::npos || file.name == "." || file.name == "..")
					return false;
				std::transform(file.sha256.begin(), file.sha256.end(), file.sha256.begin(),
					[](unsigned char c) { return (char)std::tolower(c); });
				if (file.sha256.find_first_not_of("0123456789abcdef") != std::string::npos)
					return false;
				manifest.files.push_back(file);
			}
			// unknown keys are ignored so the format can grow
		}
		if (!manifest.IsValid())
			return false;
		out = manifest;
		return true;
	}
};

// Local development builds are versioned "<x.y.z>-dev"; they are never
// replaced by the auto-updater unless explicitly asked (--update).
inline bool IsDevVersion(const std::string& version)
{
	return version.empty() || version == "dev" || version.find("-dev") != std::string::npos;
}

// CI versions master builds as "nightly-YYYYMMDD-<sha>".
inline bool IsNightlyVersion(const std::string& version)
{
	return version.compare(0, 8, "nightly-") == 0;
}

// The update channel a build belongs to: nightly builds follow the rolling
// "nightly" pre-release, everything else the latest stable release.
inline const char* DefaultChannelFor(const std::string& version)
{
	return IsNightlyVersion(version) ? "nightly" : "stable";
}
