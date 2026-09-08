// Conversion of FiveM's universal native crossmap
// (code/components/rage-scripting-five/include/CrossMapping_Universal.h in
// https://github.com/citizenfx/fivem) into the natives-<version>.txt format
// orange-core reads (shared/NativeCrossmap.h).
//
// The file is a list of rows "{ { hash0, hash1, ... hash27 } }," with one
// column per hash reshuffle Rockstar did (FiveM TableBuilder.cpp): column 0
// is build 323 (the canonical hash), the next columns belong to the builds
// 350, 372, ... 2802, 2944, and since 1.0.2944 the hashes did not change any
// more, so the last column serves every newer build. A native added later
// has zeros before the column of the build it appeared in; its first non-zero
// entry is the canonical hash every natives database uses.
//
// Header-only and platform independent (used by the launcher and the tests;
// tools/natives/crossmap_from_fivem.py is the same algorithm in Python).
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace orange
{
	struct UniversalCrossmapStats
	{
		int column;             // column used for the requested build
		size_t rows;            // rows with at least two hashes
		size_t translations;    // lines written
		size_t identical;       // canonical == build hash, nothing to translate
		size_t absent;          // native does not exist in the build (or no canonical)

		UniversalCrossmapStats() : column(0), rows(0), translations(0), identical(0), absent(0) {}
	};

	// Column of the universal table for a game build (third component of the
	// version, e.g. 3889 for 1.0.3889.0): the number of reshuffles up to it.
	inline int UniversalCrossmapColumn(int build)
	{
		static const int versions[] = { 350, 372, 393, 463, 505, 573, 617, 678, 757, 791, 877, 944, 1011, 1103, 1180,
			1290, 1365, 1493, 1604, 1737, 1868, 2060, 2189, 2372, 2545, 2802, 2944 };
		int column = 0;
		for (size_t i = 0; i < sizeof(versions) / sizeof(versions[0]); ++i)
			if (build >= versions[i])
				++column;
		return column;
	}

	inline bool ParseHex16(const char* s, uint64_t& out)
	{
		uint64_t value = 0;
		for (int i = 0; i < 16; ++i)
		{
			char c = s[i];
			int digit;
			if (c >= '0' && c <= '9')
				digit = c - '0';
			else if (c >= 'a' && c <= 'f')
				digit = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				digit = c - 'A' + 10;
			else
				return false;
			value = (value << 4) | (uint64_t)digit;
		}
		out = value;
		return true;
	}

	// Appends "0x<canonical> 0x<build hash>\n" lines to `out` for every native
	// of `build` whose hash differs from the canonical one. Returns false when
	// the input contains no rows at all.
	inline bool ConvertUniversalCrossmap(const std::string& text, int build, std::string& out, UniversalCrossmapStats& stats, std::string* error = NULL)
	{
		stats = UniversalCrossmapStats();
		stats.column = UniversalCrossmapColumn(build);
		std::vector<uint64_t> row;
		const size_t size = text.size();
		size_t pos = 0;
		char line[64];
		while (pos < size)
		{
			size_t end = text.find('\n', pos);
			if (end == std::string::npos)
				end = size;
			row.clear();
			size_t i = pos;
			while (i + 18 <= end)
			{
				uint64_t value;
				if (text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'X') && ParseHex16(text.c_str() + i + 2, value))
				{
					row.push_back(value);
					i += 18;
				}
				else
					++i;
			}
			if (row.size() >= 2)
			{
				++stats.rows;
				uint64_t canonical = 0;
				for (size_t k = 0; k < row.size(); ++k)
					if (row[k])
					{
						canonical = row[k];
						break;
					}
				uint64_t value = (size_t)stats.column < row.size() ? row[stats.column] : 0;
				if (!canonical || !value)
					++stats.absent;
				else if (value == canonical)
					++stats.identical;
				else
				{
					snprintf(line, sizeof(line), "0x%016llX 0x%016llX\n", (unsigned long long)canonical, (unsigned long long)value);
					out += line;
					++stats.translations;
				}
			}
			pos = end + 1;
		}
		if (!stats.rows)
		{
			if (error)
				*error = "no crossmap rows (lines with 0x... hashes) found in the input";
			return false;
		}
		return true;
	}
}
