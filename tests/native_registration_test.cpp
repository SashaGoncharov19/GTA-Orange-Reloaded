// Pins the obfuscated native registration layout (shared/NativeRegistrationObf.h)
// to the decoding GTA V 1.0.3889.0 performs in its own lookup: hash entries of
// 16 bytes from +0x54, key = (uint32)entry ^ dword(entry + 8).
#include "NativeRegistrationObf.h"

#include <cstdio>
#include <cstring>
#include <vector>

using orange::NativeRegistrationObf;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAILED: %s (line %d)\n", #cond, __LINE__); ++g_failures; } } while (0)

// Encodes the way the game does with arbitrary key material in the second
// qword (not just zero), so the reader is tested against the real scheme.
static void PutEntry(NativeRegistrationObf& reg, uint32_t index, uint64_t hash, uint32_t material)
{
	uint8_t* entry = reg.raw + NativeRegistrationObf::HashesOffset + NativeRegistrationObf::EntrySize * index;
	std::memcpy(entry + 8, &material, 4);
	uint32_t key = (uint32_t)(uintptr_t)entry ^ material;
	NativeRegistrationObf::EncodeQword(entry, hash, key);
}

int main()
{
	// Two records, chained, allocated with 16-byte alignment like the game heap.
	std::vector<uint8_t> storage(2 * sizeof(NativeRegistrationObf) + 64);
	uintptr_t p = ((uintptr_t)storage.data() + 15) & ~(uintptr_t)15;
	NativeRegistrationObf* a = (NativeRegistrationObf*)p;
	NativeRegistrationObf* b = (NativeRegistrationObf*)(p + ((sizeof(NativeRegistrationObf) + 15) & ~(size_t)15));
	std::memset(a, 0xCC, sizeof(*a));
	std::memset(b, 0xCC, sizeof(*b));

	// layout constants match the disassembly
	CHECK(NativeRegistrationObf::HandlersOffset == 0x10);
	CHECK(NativeRegistrationObf::NumEntriesOffset == 0x48);
	CHECK(NativeRegistrationObf::HashesOffset == 0x54);
	CHECK(sizeof(NativeRegistrationObf) == 0x54 + 7 * 16);

	a->setNextRegistration(b);
	b->setNextRegistration(nullptr);
	a->setNumEntries(7);
	b->setNumEntries(3);
	const uint64_t hashes[7] = { 0x259BE71D8A81D4FAULL, 0x75EAB09F5E974116ULL, 0xDCE42B3C644D1A4EULL,
		0x4A8C381C258A124DULL, 0x0000000000000001ULL, 0xFFFFFFFFFFFFFFFFULL, 0x8000000000000000ULL };
	for (uint32_t i = 0; i < 7; ++i)
	{
		PutEntry(*a, i, hashes[i], 0x1234567u * (i + 1));
		a->setHandler(i, (void*)(uintptr_t)(0x140000000ULL + 0x1000 * i));
	}
	for (uint32_t i = 0; i < 3; ++i)
	{
		b->setHash(i, hashes[i] ^ 0xABCDULL);
		b->setHandler(i, (void*)(uintptr_t)(0x140100000ULL + i));
	}

	CHECK(a->getNextRegistration() == b);
	CHECK(b->getNextRegistration() == nullptr);
	CHECK(a->getNumEntries() == 7);
	CHECK(b->getNumEntries() == 3);
	for (uint32_t i = 0; i < 7; ++i)
	{
		CHECK(a->getHash(i) == hashes[i]);
		CHECK(a->getHandler(i) == (void*)(uintptr_t)(0x140000000ULL + 0x1000 * i));
	}
	for (uint32_t i = 0; i < 3; ++i)
		CHECK(b->getHash(i) == (hashes[i] ^ 0xABCDULL));

	// A reader that assumed +0x58 would see something else for every entry.
	for (uint32_t i = 0; i < 7; ++i)
	{
		const uint8_t* wrong = a->raw + 0x58 + 16 * i;
		uint32_t key = (uint32_t)(uintptr_t)wrong ^ NativeRegistrationObf::Dword(wrong + 8);
		CHECK(NativeRegistrationObf::DecodeQword(wrong, key) != hashes[i]);
	}

	if (g_failures)
	{
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("native_registration_test: all checks passed\n");
	return 0;
}
