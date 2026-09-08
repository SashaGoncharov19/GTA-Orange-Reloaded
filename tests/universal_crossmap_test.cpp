// Tests for shared/NativeCrossmapUniversal.h (FiveM universal table -> natives-<version>.txt).
#include "NativeCrossmapUniversal.h"
#include "NativeCrossmap.h"

#include <cstdio>
#include <string>

using namespace orange;

static int g_failures = 0;

#define CHECK(cond) do { if (!(cond)) { std::printf("FAILED: %s (line %d)\n", #cond, __LINE__); ++g_failures; } } while (0)

int main()
{
	CHECK(UniversalCrossmapColumn(323) == 0);
	CHECK(UniversalCrossmapColumn(350) == 1);
	CHECK(UniversalCrossmapColumn(944) == 12);
	CHECK(UniversalCrossmapColumn(2802) == 26);
	CHECK(UniversalCrossmapColumn(2944) == 27);
	CHECK(UniversalCrossmapColumn(3889) == 27);

	// three columns: build 323, 350, 372
	std::string text =
		"{ { 0x0000000000000001, 0x0000000000000002, 0x0000000000000003 } },\r\n"
		"{ { 0x0000000000000000, 0x00000000000000AA, 0x00000000000000AA } },\n"   // added in 350, unchanged since
		"{ { 0x00000000000000BB, 0x0000000000000000, 0x0000000000000000 } },\n"   // removed after 323
		"// a comment line without hashes\n"
		"{ { 0x1234567890ABCDEF, 0xFEDCBA0987654321, 0x0F0F0F0F0F0F0F0F } }";      // no trailing newline

	std::string out;
	UniversalCrossmapStats stats;
	CHECK(ConvertUniversalCrossmap(text, 372, out, stats));
	CHECK(stats.column == 2);
	CHECK(stats.rows == 4);
	CHECK(stats.translations == 2);
	CHECK(stats.identical == 1);
	CHECK(stats.absent == 1);

	NativeCrossmap map;
	CHECK(map.Parse(out));
	CHECK(map.errors.empty());
	uint64_t value = 0;
	CHECK(map.Translate(1, value) && value == 3);
	CHECK(map.Translate(0x1234567890ABCDEFULL, value) && value == 0x0F0F0F0F0F0F0F0FULL);
	CHECK(!map.Translate(0xAA, value));   // identical pairs are not written

	// build 350 = column 1
	out.clear();
	CHECK(ConvertUniversalCrossmap(text, 350, out, stats));
	CHECK(stats.column == 1 && stats.translations == 2 && stats.identical == 1 && stats.absent == 1);

	// a column the rows do not have: everything counts as absent
	out.clear();
	CHECK(ConvertUniversalCrossmap(text, 3889, out, stats));
	CHECK(stats.column == 27 && stats.translations == 0 && stats.absent == 4);

	std::string error;
	CHECK(!ConvertUniversalCrossmap("nothing here\n", 3889, out, stats, &error));
	CHECK(!error.empty());

	if (g_failures)
	{
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("universal_crossmap_test: all checks passed\n");
	return 0;
}
