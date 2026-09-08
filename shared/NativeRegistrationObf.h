// The obfuscated native registration record GTA V uses since 1.0.1290: a
// singly linked list per hash bucket, seven natives per record. The next
// pointer, the entry count and every hash are XOR-ed with (the low 32 bits
// of) their own address, so they can only be read in place.
//
// The layout is the one the game's own lookup uses (1.0.3889.0, the
// function called with rcx = table and rdx = hash right after
// "lea rcx, [registration table]"):
//
//   movzx eax, dl           ; bucket = hash & 0xFF
//   lea   r9, [r8+0x48]     ; numEntries1 / numEntries2
//   lea   rdx, [r8+0x54]    ; hash entries, 16 bytes each, FROM +0x54
//   mov   r11d, [rdx+8]     ; key = dword(entry+8) ^ (uint32)entry
//   ...  two dwords at entry+0 / entry+4 XOR key = the hash
//
// +0x54 is not 8-byte aligned; a struct with a uint64_t hashes[] member
// lands on +0x58 and decodes garbage (every lookup then misses although a
// full walk still counts the entries and returns the right handlers, which
// is exactly how it went unnoticed). FiveM's rage-scripting-five reads the
// same 0x54. Everything here is plain arithmetic on the raw bytes.
#pragma once
#include <cstdint>
#include <cstring>

namespace orange
{
	struct NativeRegistrationObf
	{
		enum { HandlersOffset = 0x10, NumEntriesOffset = 0x48, HashesOffset = 0x54, MaxEntries = 7, EntrySize = 16 };

		uint8_t raw[HashesOffset + MaxEntries * EntrySize];

		static uint32_t Dword(const uint8_t* at)
		{
			uint32_t v;
			std::memcpy(&v, at, 4);
			return v;
		}

		// Two dwords at `at` XOR-ed with `key`, low first.
		static uint64_t DecodeQword(const uint8_t* at, uint32_t key)
		{
			uint64_t low = Dword(at) ^ key;
			uint64_t high = Dword(at + 4) ^ key;
			return (high << 32) | low;
		}

		const NativeRegistrationObf* getNextRegistration() const
		{
			uint32_t key = (uint32_t)(uintptr_t)raw ^ Dword(raw + 8);
			return (const NativeRegistrationObf*)(uintptr_t)DecodeQword(raw, key);
		}

		uint32_t getNumEntries() const
		{
			const uint8_t* at = raw + NumEntriesOffset;
			return (uint32_t)(uintptr_t)at ^ Dword(at) ^ Dword(at + 4);
		}

		uint64_t getHash(uint32_t index) const
		{
			const uint8_t* entry = raw + HashesOffset + EntrySize * index;
			uint32_t key = (uint32_t)(uintptr_t)entry ^ Dword(entry + 8);
			return DecodeQword(entry, key);
		}

		void* getHandler(uint32_t index) const
		{
			void* handler;
			std::memcpy(&handler, raw + HandlersOffset + 8 * index, sizeof(handler));
			return handler;
		}

		// --- writers, for tests and for registering natives of our own ------
		void setNextRegistration(const NativeRegistrationObf* next)
		{
			uint32_t key = (uint32_t)(uintptr_t)raw;      // second qword = 0
			std::memset(raw + 8, 0, 8);
			EncodeQword(raw, (uint64_t)(uintptr_t)next, key);
		}

		void setNumEntries(uint32_t n)
		{
			uint8_t* at = raw + NumEntriesOffset;
			uint32_t v = (uint32_t)(uintptr_t)at ^ n;       // numEntries2 = 0
			std::memcpy(at, &v, 4);
			std::memset(at + 4, 0, 4);
		}

		void setHash(uint32_t index, uint64_t hash)
		{
			uint8_t* entry = raw + HashesOffset + EntrySize * index;
			std::memset(entry + 8, 0, 8);                   // key material = 0
			EncodeQword(entry, hash, (uint32_t)(uintptr_t)entry);
		}

		void setHandler(uint32_t index, void* handler)
		{
			std::memcpy(raw + HandlersOffset + 8 * index, &handler, sizeof(handler));
		}

		static void EncodeQword(uint8_t* at, uint64_t value, uint32_t key)
		{
			uint32_t low = (uint32_t)value ^ key;
			uint32_t high = (uint32_t)(value >> 32) ^ key;
			std::memcpy(at, &low, 4);
			std::memcpy(at + 4, &high, 4);
		}
	};
}
