// Tests for shared/NativeCrossmap.h and the generated reference crossmap
// (orange-core/Core/NativeCrossmap_Reference.h).
#include "NativeCrossmap.h"
#include "NativeCrossmap_Reference.h"

#include <cstdio>
#include <set>
#include <string>

using namespace orange;

static int g_failures = 0;

#define CHECK(cond) do { if (!(cond)) { std::printf("FAILED: %s (line %d)\n", #cond, __LINE__); ++g_failures; } } while (0)

int main()
{
	// --- ParseHash64 --------------------------------------------------------
	{
		uint64_t v = 0;
		CHECK(ParseHash64("0x4F8644AF03D0E0D6", v) && v == 0x4F8644AF03D0E0D6ULL);
		CHECK(ParseHash64("4f8644af03d0e0d6", v) && v == 0x4F8644AF03D0E0D6ULL);
		CHECK(ParseHash64("  0X10 ", v) && v == 0x10);
		CHECK(!ParseHash64("", v));
		CHECK(!ParseHash64("0x", v));
		CHECK(!ParseHash64("0xZZ", v));
		CHECK(!ParseHash64("0x12345678901234567", v));
	}

	// --- NativeCrossmap::Parse ----------------------------------------------
	{
		NativeCrossmap m;
		std::string text =
			"; natives for 1.0.3889.0\r\n"
			"# canonical build name\n"
			"0x4F8644AF03D0E0D6 0x1111111111111111 PLAYER_ID\n"
			"0xD80958FC74E988A6\t0x2222222222222222\n"
			"  0xAAAA 0xBBBB   ; trailing comment\n"
			"\n";
		CHECK(m.Parse(text));
		CHECK(m.errors.empty());
		CHECK(m.Size() == 3);
		uint64_t out = 0;
		CHECK(m.Translate(0x4F8644AF03D0E0D6ULL, out) && out == 0x1111111111111111ULL);
		CHECK(m.Translate(0xD80958FC74E988A6ULL, out) && out == 0x2222222222222222ULL);
		CHECK(m.Translate(0xAAAAULL, out) && out == 0xBBBBULL);
		CHECK(!m.Translate(0x1234ULL, out));

		CHECK(!m.Parse("0x1 0x2\nonly-one\n0xZZ 0x3\n0x4 0x5\n"));
		CHECK(m.errors.size() == 2);
		CHECK(m.Size() == 2);

		m.Add(0x9, 0x10);
		CHECK(m.Translate(0x9, out) && out == 0x10);
	}

	// --- generated reference crossmap ---------------------------------------
	{
		CHECK(g_nativeCrossmapReferenceCount > 5000);
		CHECK(sizeof(g_nativeCrossmapReference) / sizeof(g_nativeCrossmapReference[0]) == g_nativeCrossmapReferenceCount);
		std::set<uint64_t> canonicals;
		NativeCrossmap reference;
		for (size_t i = 0; i < g_nativeCrossmapReferenceCount; ++i)
		{
			uint64_t canonical = g_nativeCrossmapReference[i][0];
			uint64_t build = g_nativeCrossmapReference[i][1];
			CHECK(canonical != 0 && build != 0);
			CHECK(canonicals.insert(canonical).second);   // unique
			reference.Add(canonical, build);
		}
		uint64_t out = 0;
		// PLAYER_ID: canonical 0x4F8644AF03D0E0D6, January 2017 build 0x0C1D3C552325765B
		CHECK(reference.Translate(0x4F8644AF03D0E0D6ULL, out) && out == 0x0C1D3C552325765BULL);
		// PLAYER_PED_ID: canonical 0xD80958FC74E988A6, January 2017 build 0xA0081090911D13E5
		CHECK(reference.Translate(0xD80958FC74E988A6ULL, out) && out == 0xA0081090911D13E5ULL);
	}

	if (g_failures)
	{
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("natives_crossmap_test: all checks passed\n");
	return 0;
}
