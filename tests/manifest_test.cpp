// Tests for Launcher/UpdateManifest.h (the only platform independent piece of
// the auto-updater, so it can be checked on Linux as well).
#include "UpdateManifest.h"

#include <cstdio>
#include <string>

static int g_failures = 0;

#define CHECK(cond) do { if (!(cond)) { std::printf("FAILED: %s (line %d)\n", #cond, __LINE__); ++g_failures; } } while (0)

static const char* kHashA = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static const char* kHashB = "FEDCBA9876543210FEDCBA9876543210FEDCBA9876543210FEDCBA9876543210";

int main()
{
	// --- happy path, comments, CRLF, unknown keys, uppercase hashes ----------
	{
		std::string text =
			"# GTA:Orange client manifest\r\n"
			"version 0.2.0\r\n"
			"file orange-core.dll " + std::string(kHashA) + " 123456\r\n"
			"file Launcher.exe " + std::string(kHashB) + " 42  # trailing comment\r\n"
			"future-key something\r\n";
		UpdateManifest m;
		CHECK(UpdateManifest::Parse(text, m));
		CHECK(m.version == "0.2.0");
		CHECK(m.files.size() == 2);
		if (m.files.size() == 2)
		{
			CHECK(m.files[0].name == "orange-core.dll");
			CHECK(m.files[0].sha256 == kHashA);
			CHECK(m.files[0].size == 123456);
			CHECK(m.files[1].name == "Launcher.exe");
			CHECK(m.files[1].sha256 == "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210");
			CHECK(m.files[1].size == 42);
		}
	}

	// --- nightly ids are plain strings --------------------------------------
	{
		UpdateManifest m;
		CHECK(UpdateManifest::Parse("version nightly-20260908-f15d9d3\nfile Launcher.exe " + std::string(kHashA) + " 1\n", m));
		CHECK(m.version == "nightly-20260908-f15d9d3");
	}

	// --- rejected manifests ---------------------------------------------------
	{
		UpdateManifest m;
		CHECK(!UpdateManifest::Parse("", m));
		CHECK(!UpdateManifest::Parse("version 1.0\n", m));                                          // no files
		CHECK(!UpdateManifest::Parse("file a.dll " + std::string(kHashA) + " 1\n", m));               // no version
		CHECK(!UpdateManifest::Parse("version 1.0\nfile a.dll abcdef 1\n", m));                       // short hash
		CHECK(!UpdateManifest::Parse("version 1.0\nfile a.dll " + std::string(kHashA).substr(0, 63) + "z 1\n", m)); // non hex
		CHECK(!UpdateManifest::Parse("version 1.0\nfile ../evil.dll " + std::string(kHashA) + " 1\n", m));
		CHECK(!UpdateManifest::Parse("version 1.0\nfile sub/evil.dll " + std::string(kHashA) + " 1\n", m));
		CHECK(!UpdateManifest::Parse("version 1.0\nfile C:evil.dll " + std::string(kHashA) + " 1\n", m));
		CHECK(!UpdateManifest::Parse("version 1.0\nfile .. " + std::string(kHashA) + " 1\n", m));
		// a rejected manifest must not touch the output
		UpdateManifest untouched;
		untouched.version = "keep";
		CHECK(!UpdateManifest::Parse("garbage", untouched));
		CHECK(untouched.version == "keep");
	}

	// --- development builds are not auto-updated -----------------------------
	CHECK(IsDevVersion("0.2.0-dev"));
	CHECK(IsDevVersion("dev"));
	CHECK(IsDevVersion(""));
	CHECK(!IsDevVersion("0.2.0"));
	CHECK(!IsDevVersion("nightly-20260908-f15d9d3"));

	CHECK(IsNightlyVersion("nightly-20260908-f15d9d3"));
	CHECK(!IsNightlyVersion("0.2.0"));
	CHECK(!IsNightlyVersion("nightly"));
	CHECK(!IsNightlyVersion(""));
	CHECK(std::string(DefaultChannelFor("nightly-20260908-f15d9d3")) == "nightly");
	CHECK(std::string(DefaultChannelFor("0.2.0")) == "stable");
	CHECK(std::string(DefaultChannelFor("0.2.0-dev")) == "stable");

	if (g_failures == 0)
		std::printf("manifest_test: all checks passed\n");
	return g_failures == 0 ? 0 : 1;
}
