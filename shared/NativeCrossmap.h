// NativeCrossmap.h: translation between canonical native hashes and the
// hashes a particular game build registers its natives under.
//
// Rockstar reshuffles the native hashes with every game update. Scripts and
// databases (https://docs.fivem.net/natives) use the canonical hash; the game
// only knows the hash of its own build. orange-core calls natives by the
// canonical hash and translates through one of these tables:
//
//   * the reference build:  orange-core/Core/NativeCrossmap_Reference.h,
//     generated from Natives.h (tools/natives/canonicalize_natives.py)
//   * any other build:      natives-<game version>.txt next to orange-core.dll
//
// File format, one native per line, ';' and '#' start comments:
//
//   0x4F8644AF03D0E0D6 0x0C1D3C552325765B PLAYER_ID   ; canonical, build hash, optional name
//
// No Windows headers on purpose, so tests/natives_crossmap_test.cpp runs on Linux.
#pragma once

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include "OffsetSpec.h"

namespace orange
{

// "0x4F8644AF03D0E0D6" or "4F8644AF03D0E0D6" (up to 16 hex digits).
inline bool ParseHash64(const std::string& text, uint64_t& out)
{
	std::string t = TrimCopy(text);
	if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X'))
		t = t.substr(2);
	if (t.empty() || t.size() > 16)
		return false;
	for (size_t i = 0; i < t.size(); ++i)
		if (!std::isxdigit((unsigned char)t[i]))
			return false;
	out = std::strtoull(t.c_str(), NULL, 16);
	return true;
}

struct NativeCrossmap
{
	std::unordered_map<uint64_t, uint64_t> map;   // canonical -> build hash
	std::vector<std::string> errors;              // "line N: ..." for skipped lines

	bool Parse(const std::string& text)
	{
		map.clear();
		errors.clear();
		size_t start = 0;
		size_t lineNo = 0;
		while (start <= text.size())
		{
			size_t end = text.find('\n', start);
			if (end == std::string::npos) end = text.size();
			std::string line = TrimCopy(IniFile::StripComment(text.substr(start, end - start)));
			start = end + 1;
			++lineNo;
			if (line.empty())
				continue;

			size_t sp = line.find_first_of(" \t");
			if (sp == std::string::npos)
			{
				errors.push_back("line " + std::to_string(lineNo) + ": expected '<canonical hash> <build hash>'");
				continue;
			}
			std::string second = TrimCopy(line.substr(sp));
			size_t sp2 = second.find_first_of(" \t");
			if (sp2 != std::string::npos)
				second = second.substr(0, sp2);

			uint64_t canonical = 0, build = 0;
			if (!ParseHash64(line.substr(0, sp), canonical) || !ParseHash64(second, build))
			{
				errors.push_back("line " + std::to_string(lineNo) + ": bad hash");
				continue;
			}
			map[canonical] = build;
		}
		return errors.empty();
	}

	void Add(uint64_t canonical, uint64_t build) { map[canonical] = build; }
	size_t Size() const { return map.size(); }

	bool Translate(uint64_t canonical, uint64_t& out) const
	{
		std::unordered_map<uint64_t, uint64_t>::const_iterator it = map.find(canonical);
		if (it == map.end())
			return false;
		out = it->second;
		return true;
	}
};

} // namespace orange
