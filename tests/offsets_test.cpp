// Tests for shared/OffsetSpec.h: the offsets.ini reader, the byte pattern
// scanner and the value syntax used by orange-core's GameOffsets.
#include "OffsetSpec.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace orange;

static int g_failures = 0;

#define CHECK(cond) do { if (!(cond)) { std::printf("FAILED: %s (line %d)\n", #cond, __LINE__); ++g_failures; } } while (0)

int main()
{
	// --- IniFile -------------------------------------------------------------
	{
		IniFile ini;
		std::string text =
			"; GTA:Orange offsets\r\n"
			"# another comment style\n"
			"Global = 1\n"
			"[1.0.1290.1]\n"
			"ShowAbilityBar = 0x1F26D4   ; inline comment\n"
			"  EscFreeze=disabled\n"
			"ScrThreadId = 89 15 ? ? ? ? 48 8B 0C D8 @ 0\n"
			"[Default]\n"
			"SnowPatch = off\n"
			"[empty]\n";
		CHECK(ini.Parse(text));
		CHECK(ini.errors.empty());
		CHECK(ini.HasSection("1.0.1290.1"));
		CHECK(ini.HasSection("default"));
		CHECK(ini.HasSection("DEFAULT"));
		CHECK(ini.HasSection("empty"));
		CHECK(!ini.HasSection("1.0.944.2"));
		const std::string* v = ini.Get("1.0.1290.1", "showabilitybar");
		CHECK(v && *v == "0x1F26D4");
		v = ini.Get("1.0.1290.1", "EscFreeze");
		CHECK(v && *v == "disabled");
		v = ini.Get("1.0.1290.1", "ScrThreadId");
		CHECK(v && *v == "89 15 ? ? ? ? 48 8B 0C D8 @ 0");
		v = ini.Get("", "global");
		CHECK(v && *v == "1");
		CHECK(ini.Get("default", "SnowPatch") != NULL);
		CHECK(ini.Get("default", "missing") == NULL);
		CHECK(ini.Get("nosuchsection", "SnowPatch") == NULL);
	}

	// --- IniFile: malformed lines are reported but do not abort ---------------
	{
		IniFile ini;
		CHECK(!ini.Parse("[1.0.1\nGood = 0x1\nno equals here\n= 0x2\n"));
		CHECK(ini.errors.size() == 3);
		CHECK(ini.Get("", "good") != NULL);
	}

	// --- BytePattern: parse ---------------------------------------------------
	{
		BytePattern p;
		CHECK(p.Parse("48 8B ? ?? E8 ff"));
		CHECK(p.Length() == 6);
		CHECK(p.fixed[0] && p.fixed[1] && !p.fixed[2] && !p.fixed[3] && p.fixed[4] && p.fixed[5]);
		CHECK(p.bytes[5] == 0xFF);
		CHECK(p.ToString() == "48 8B ? ? E8 FF");
		CHECK(!p.Parse(""));
		CHECK(!p.Parse("4G 8B"));
		CHECK(!p.Parse("488B"));
		CHECK(p.Parse("  90  "));
		CHECK(p.Length() == 1);
	}

	// --- BytePattern: find / count -------------------------------------------
	{
		const uint8_t data[] = {
			0x90, 0x90, 0x48, 0x8B, 0x05, 0x11, 0x22, 0x33, 0x44, 0xC3,
			0x48, 0x8B, 0x05, 0x55, 0x66, 0x77, 0x88, 0xC3, 0x48, 0x8B };
		const size_t size = sizeof(data);
		BytePattern p;
		CHECK(p.Parse("48 8B 05 ? ? ? ? C3"));
		CHECK(p.Find(data, size) == 2);
		CHECK(p.Find(data, size, 3) == 10);
		CHECK(p.Find(data, size, 11) == BytePattern::npos);
		CHECK(p.Count(data, size) == 2);
		CHECK(p.Count(data, size, 1) == 1);

		// Match at the very end of the buffer.
		BytePattern tail;
		CHECK(tail.Parse("C3 48 8B"));
		CHECK(tail.Find(data, size) == 9);
		CHECK(tail.Find(data, size, 10) == 17);
		CHECK(tail.Find(data, size, 18) == BytePattern::npos);

		// Pattern starting with wildcards: the anchor is the first fixed byte.
		BytePattern lead;
		CHECK(lead.Parse("? ? 05 11"));
		CHECK(lead.Find(data, size) == 2);

		// Only wildcards match anywhere.
		BytePattern any;
		CHECK(any.Parse("? ?"));
		CHECK(any.Find(data, size, 5) == 5);

		// Longer than the buffer.
		BytePattern longer;
		std::string text;
		for (size_t i = 0; i < size + 1; ++i) text += "90 ";
		CHECK(longer.Parse(text));
		CHECK(longer.Find(data, size) == BytePattern::npos);

		// No match at all.
		BytePattern none;
		CHECK(none.Parse("AA BB"));
		CHECK(none.Find(data, size) == BytePattern::npos);
		CHECK(none.Count(data, size) == 0);
	}

	// --- OffsetSpec -----------------------------------------------------------
	{
		OffsetSpec s;
		std::string err;
		CHECK(s.Parse("0x1F26D4", &err));
		CHECK(s.kind == OffsetSpec::Rva && s.rva == 0x1F26D4);

		CHECK(s.Parse("0X10"));
		CHECK(s.kind == OffsetSpec::Rva && s.rva == 0x10);

		CHECK(s.Parse("disabled"));
		CHECK(s.kind == OffsetSpec::Disabled);
		CHECK(s.Parse("OFF"));
		CHECK(s.kind == OffsetSpec::Disabled);
		CHECK(s.Parse("-"));
		CHECK(s.kind == OffsetSpec::Disabled);

		CHECK(s.Parse("scan"));
		CHECK(s.kind == OffsetSpec::Scan);

		CHECK(s.Parse("48 8B 0D ? ? ? ? E8 @ -7", &err));
		CHECK(s.kind == OffsetSpec::Pattern);
		CHECK(s.delta == -7);
		CHECK(s.pattern.Length() == 8);

		CHECK(s.Parse("E8 ? ? ? ? 8B CB @ 0x10"));
		CHECK(s.kind == OffsetSpec::Pattern && s.delta == 16);

		CHECK(s.Parse("E8 ? ? ? ? 8B CB"));
		CHECK(s.kind == OffsetSpec::Pattern && s.delta == 0);

		// Several candidates separated by '|', each with its own delta.
		CHECK(s.Parse("48 8B C8 EB ? 33 C9 48 8B 05 @ 7 | 48 8B C8 EB 03 49 8B CD 48 8B 05 @ 8 | 89 15 ? ? ? ? 48 8B 0C D8", &err));
		CHECK(s.kind == OffsetSpec::Pattern);
		CHECK(s.candidates.size() == 3);
		if (s.candidates.size() == 3)
		{
			CHECK(s.candidates[0].delta == 7 && s.candidates[0].pattern.Length() == 10);
			CHECK(s.candidates[1].delta == 8 && s.candidates[1].pattern.Length() == 11);
			CHECK(s.candidates[2].delta == 0 && s.candidates[2].pattern.Length() == 10);
		}
		CHECK(s.pattern.Length() == 10 && s.delta == 7);
		CHECK(!s.Parse("48 8B | ", &err));
		CHECK(!s.Parse("48 8B | ZZ", &err));
		CHECK(!s.Parse("48 8B @ 1 | 90 @ x", &err));

		std::vector<PatternCandidate> c;
		CHECK(ParsePatternCandidates("FF 0D ? ? ? ? 48 8B F9", -4, c) && c.size() == 1 && c[0].delta == -4);
		CHECK(ParsePatternCandidates("AA @ 1 | BB", 5, c) && c.size() == 2 && c[0].delta == 1 && c[1].delta == 5);
		CHECK(!ParsePatternCandidates("", 0, c));
		CHECK(!ParsePatternCandidates("|", 0, c));

		CHECK(!s.Parse("", &err));
		CHECK(!s.Parse("0xZZ", &err));
		CHECK(!err.empty());
		CHECK(!s.Parse("hello world", &err));
		CHECK(!s.Parse("48 8B @ abc", &err));
	}

	// --- HexString ------------------------------------------------------------
	CHECK(HexString(0x1F26D4) == "0x1F26D4");
	CHECK(HexString(0) == "0x0");

	if (g_failures)
	{
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("offsets_test: all checks passed\n");
	return 0;
}
