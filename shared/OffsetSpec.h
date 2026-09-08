// OffsetSpec.h: portable helpers behind orange-core's game offset handling.
//
//   IniFile      minimal INI reader (sections, "key = value", ';' / '#' comments)
//   BytePattern  "48 8B ? ? E8" style byte patterns: parse, match, find
//   OffsetSpec   how a single value of offsets.ini is interpreted
//
// No Windows headers on purpose, so tests/offsets_test.cpp can run on Linux.
// See docs/UPDATING_OFFSETS.md for the file format.
#pragma once

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace orange
{

inline std::string TrimCopy(const std::string& s)
{
	size_t b = 0, e = s.size();
	while (b < e && std::isspace((unsigned char)s[b])) ++b;
	while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
	return s.substr(b, e - b);
}

inline std::string LowerCopy(const std::string& s)
{
	std::string r(s);
	for (size_t i = 0; i < r.size(); ++i)
		r[i] = (char)std::tolower((unsigned char)r[i]);
	return r;
}

inline std::string HexString(uint64_t value)
{
	char buf[32];
	std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)value);
	return buf;
}

// ---------------------------------------------------------------------------
// IniFile
// ---------------------------------------------------------------------------
struct IniFile
{
	typedef std::map<std::string, std::string> Section;   // key (lower case) -> value

	std::map<std::string, Section> sections;              // name (lower case) -> section
	std::vector<std::string> errors;                      // "line N: ..." for lines that were skipped

	static std::string StripComment(const std::string& line)
	{
		size_t pos = line.find_first_of(";#");
		return pos == std::string::npos ? line : line.substr(0, pos);
	}

	bool Parse(const std::string& text)
	{
		sections.clear();
		errors.clear();
		std::string current;
		size_t lineNo = 0;
		size_t start = 0;
		while (start <= text.size())
		{
			size_t end = text.find('\n', start);
			if (end == std::string::npos) end = text.size();
			std::string line = text.substr(start, end - start);
			start = end + 1;
			++lineNo;

			line = TrimCopy(StripComment(line));
			if (line.empty())
				continue;
			if (line[0] == '[')
			{
				size_t close = line.find(']');
				if (close == std::string::npos)
				{
					errors.push_back("line " + std::to_string(lineNo) + ": unterminated section header");
					continue;
				}
				current = LowerCopy(TrimCopy(line.substr(1, close - 1)));
				sections[current];   // make sure empty sections exist too
				continue;
			}
			size_t eq = line.find('=');
			if (eq == std::string::npos)
			{
				errors.push_back("line " + std::to_string(lineNo) + ": expected key = value");
				continue;
			}
			std::string key = LowerCopy(TrimCopy(line.substr(0, eq)));
			std::string value = TrimCopy(line.substr(eq + 1));
			if (key.empty())
			{
				errors.push_back("line " + std::to_string(lineNo) + ": empty key");
				continue;
			}
			sections[current][key] = value;
		}
		return errors.empty();
	}

	bool HasSection(const std::string& name) const
	{
		return sections.find(LowerCopy(name)) != sections.end();
	}

	// Returns NULL when the section or the key does not exist.
	const std::string* Get(const std::string& section, const std::string& key) const
	{
		std::map<std::string, Section>::const_iterator s = sections.find(LowerCopy(section));
		if (s == sections.end())
			return NULL;
		Section::const_iterator k = s->second.find(LowerCopy(key));
		if (k == s->second.end())
			return NULL;
		return &k->second;
	}
};

// ---------------------------------------------------------------------------
// BytePattern
// ---------------------------------------------------------------------------
struct BytePattern
{
	std::vector<uint8_t> bytes;
	std::vector<bool> fixed;      // true = byte must match, false = wildcard

	static const size_t npos = (size_t)-1;

	bool Empty() const { return bytes.empty(); }
	size_t Length() const { return bytes.size(); }

	// Accepts "48 8B ? ? E8 ?? 90". Returns false on an empty or malformed pattern.
	bool Parse(const std::string& text)
	{
		bytes.clear();
		fixed.clear();
		size_t i = 0;
		while (i < text.size())
		{
			while (i < text.size() && std::isspace((unsigned char)text[i])) ++i;
			if (i >= text.size()) break;
			size_t j = i;
			while (j < text.size() && !std::isspace((unsigned char)text[j])) ++j;
			std::string tok = text.substr(i, j - i);
			i = j;
			if (tok == "?" || tok == "??")
			{
				bytes.push_back(0);
				fixed.push_back(false);
				continue;
			}
			if (tok.size() != 2 || !std::isxdigit((unsigned char)tok[0]) || !std::isxdigit((unsigned char)tok[1]))
				return false;
			bytes.push_back((uint8_t)std::strtoul(tok.c_str(), NULL, 16));
			fixed.push_back(true);
		}
		return !bytes.empty();
	}

	std::string ToString() const
	{
		std::string out;
		for (size_t i = 0; i < bytes.size(); ++i)
		{
			if (i) out += ' ';
			if (!fixed[i]) { out += '?'; continue; }
			char b[4];
			std::snprintf(b, sizeof(b), "%02X", bytes[i]);
			out += b;
		}
		return out;
	}

	bool Matches(const uint8_t* data) const
	{
		for (size_t i = 0; i < bytes.size(); ++i)
			if (fixed[i] && data[i] != bytes[i])
				return false;
		return true;
	}

	// Index of the first match at or after startAt, npos if there is none.
	size_t Find(const uint8_t* data, size_t size, size_t startAt = 0) const
	{
		const size_t n = bytes.size();
		if (n == 0 || size < n)
			return npos;
		const size_t last = size - n;           // last valid start index
		if (startAt > last)
			return npos;

		size_t first = n;
		for (size_t i = 0; i < n; ++i)
			if (fixed[i]) { first = i; break; }
		if (first == n)                         // only wildcards
			return startAt;

		const uint8_t fb = bytes[first];
		size_t i = startAt;
		while (i <= last)
		{
			const void* hit = std::memchr(data + i + first, fb, last - i + 1);
			if (!hit)
				return npos;
			size_t pos = (size_t)((const uint8_t*)hit - data) - first;
			if (Matches(data + pos))
				return pos;
			i = pos + 1;
		}
		return npos;
	}

	// Number of matches, stops counting at `limit`.
	size_t Count(const uint8_t* data, size_t size, size_t limit = 2) const
	{
		size_t count = 0, pos = 0;
		while (count < limit)
		{
			pos = Find(data, size, pos);
			if (pos == npos) break;
			++count;
			++pos;
		}
		return count;
	}
};

// ---------------------------------------------------------------------------
// OffsetSpec: one "Name = value" line of offsets.ini
//
//   Name = 0x1F26D4                RVA relative to the GTA5.exe base
//   Name = disabled                skip this hook / patch (also: off, none, -)
//   Name = scan                    use the pattern built into orange-core
//   Name = 48 8B ? ? E8 @ -7       scan for this pattern, add -7 to the match
// ---------------------------------------------------------------------------
struct OffsetSpec
{
	enum Kind { Rva, Disabled, Scan, Pattern };

	Kind kind;
	uint64_t rva;
	BytePattern pattern;
	int delta;

	OffsetSpec() : kind(Rva), rva(0), delta(0) {}

	static bool ParseInteger(const std::string& text, long long& out)
	{
		std::string t = TrimCopy(text);
		if (t.empty()) return false;
		char* end = NULL;
		long long v = std::strtoll(t.c_str(), &end, 0);
		if (end == t.c_str() || *end != '\0') return false;
		out = v;
		return true;
	}

	bool Parse(const std::string& rawValue, std::string* error = NULL)
	{
		kind = Rva; rva = 0; delta = 0; pattern = BytePattern();
		std::string value = TrimCopy(rawValue);
		std::string lower = LowerCopy(value);
		if (value.empty())
		{
			if (error) *error = "empty value";
			return false;
		}
		if (lower == "disabled" || lower == "off" || lower == "none" || lower == "-")
		{
			kind = Disabled;
			return true;
		}
		if (lower == "scan" || lower == "pattern")
		{
			kind = Scan;
			return true;
		}
		if (lower.size() > 2 && lower[0] == '0' && lower[1] == 'x')
		{
			long long v = 0;
			if (!ParseInteger(value, v) || v < 0)
			{
				if (error) *error = "bad hexadecimal value '" + value + "'";
				return false;
			}
			kind = Rva;
			rva = (uint64_t)v;
			return true;
		}
		// Pattern, optionally followed by "@ delta".
		std::string patternText = value;
		size_t at = value.find('@');
		if (at != std::string::npos)
		{
			long long d = 0;
			if (!ParseInteger(value.substr(at + 1), d))
			{
				if (error) *error = "bad delta after '@' in '" + value + "'";
				return false;
			}
			delta = (int)d;
			patternText = value.substr(0, at);
		}
		if (!pattern.Parse(patternText))
		{
			if (error) *error = "not an address, keyword or byte pattern: '" + value + "'";
			return false;
		}
		kind = Pattern;
		return true;
	}
};

} // namespace orange
