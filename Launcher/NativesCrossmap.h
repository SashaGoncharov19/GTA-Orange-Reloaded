#pragma once
#include <string>

// natives-<game version>.txt for orange-core: the translation from canonical
// native hashes to the hashes of the running game build. When the file is
// missing next to the launcher it is generated from FiveM's public universal
// crossmap (downloaded from GitHub, not redistributed). Never blocks the
// game start: failures are logged and orange-core reports the missing file.
void EnsureNativesCrossmap(const std::wstring& installDir, const std::wstring& gameVersion);
