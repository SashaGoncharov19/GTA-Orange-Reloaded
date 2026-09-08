#include "stdafx.h"
#include "NativeCrossmap.h"
#include "NativeCrossmap_Reference.h"
#include "NativeRegistrationObf.h"

namespace
{
	// Registration layout of builds before 1.0.1290 (the reference build).
	struct NativeRegistration
	{
		NativeRegistration* nextRegistration;
		ScriptEngine::NativeHandler handlers[7];
		uint32_t numEntries;
		uint64_t hashes[7];
	};

	// Registration layout from 1.0.1290 on: shared/NativeRegistrationObf.h,
	// pinned to the game's own lookup code (hash entries from +0x54, not the
	// aligned +0x58 a struct member would give; that mistake made every
	// lookup miss while the walk still counted all 6701 natives).
	typedef orange::NativeRegistrationObf NativeRegistrationObf;

	bool g_initialized = false;
	bool g_obfuscated = false;
	void** g_table = nullptr;          // registrationTable[256]
	orange::NativeCrossmap g_crossmap;
	std::string g_source = "none";
	std::set<uint64_t> g_reported;     // canonical hashes already reported as untranslated

	// The walks below touch game memory whose layout is an assumption on
	// unknown builds; SEH keeps a wrong assumption from taking the game down.
	// These functions must not contain objects with destructors (C2712).
	ScriptEngine::NativeHandler FindPlain(void** table, uint64_t hash)
	{
		__try
		{
			int guard = 0;
			for (NativeRegistration* reg = (NativeRegistration*)table[hash & 0xFF]; reg && guard < 4096; reg = reg->nextRegistration, ++guard)
			{
				uint32_t n = reg->numEntries;
				if (n > 7)
					break;
				for (uint32_t i = 0; i < n; ++i)
					if (reg->hashes[i] == hash)
						return reg->handlers[i];
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
		return nullptr;
	}

	ScriptEngine::NativeHandler FindObf(void** table, uint64_t hash)
	{
		__try
		{
			int guard = 0;
			for (const NativeRegistrationObf* reg = (const NativeRegistrationObf*)table[hash & 0xFF]; reg && guard < 4096; reg = reg->getNextRegistration(), ++guard)
			{
				uint32_t n = reg->getNumEntries();
				if (n > 7)
					break;
				for (uint32_t i = 0; i < n; ++i)
					if (reg->getHash(i) == hash)
						return (ScriptEngine::NativeHandler)reg->getHandler(i);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
		return nullptr;
	}

	// Collects (hash, handler) of every registered native; returns the count.
	size_t WalkAll(void** table, bool obfuscated, uint64_t* hashes, uint64_t* handlers, size_t capacity)
	{
		size_t count = 0;
		__try
		{
			for (int bucket = 0; bucket < 256; ++bucket)
			{
				int guard = 0;
				if (obfuscated)
				{
					for (const NativeRegistrationObf* reg = (const NativeRegistrationObf*)table[bucket]; reg && guard < 4096; reg = reg->getNextRegistration(), ++guard)
					{
						uint32_t n = reg->getNumEntries();
						if (n > 7)
							break;
						for (uint32_t i = 0; i < n && count < capacity; ++i)
						{
							hashes[count] = reg->getHash(i);
							handlers[count] = (uint64_t)reg->getHandler(i);
							++count;
						}
					}
				}
				else
				{
					for (NativeRegistration* reg = (NativeRegistration*)table[bucket]; reg && guard < 4096; reg = reg->nextRegistration, ++guard)
					{
						uint32_t n = reg->numEntries;
						if (n > 7)
							break;
						for (uint32_t i = 0; i < n && count < capacity; ++i)
						{
							hashes[count] = reg->hashes[i];
							handlers[count] = (uint64_t)reg->handlers[i];
							++count;
						}
					}
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return count;
		}
		return count;
	}

	void** ResolveTable()
	{
		if (!g_table)
			g_table = (void**)GameMem("RegistrationTable").getOffset();
		return g_table;
	}
}

namespace NativeTable
{
	void Initialize()
	{
		if (g_initialized)
			return;
		g_initialized = true;

		int build = GameOffsets::GameBuildNumber();
		g_obfuscated = !GameOffsets::IsReferenceBuild() && (build == 0 || build >= 1290);
		log_info << "Natives: registration table layout " << (g_obfuscated ? "obfuscated (1.0.1290+)" : "plain") << std::endl;

		if (GameOffsets::IsReferenceBuild())
		{
			for (size_t i = 0; i < g_nativeCrossmapReferenceCount; ++i)
				g_crossmap.Add(g_nativeCrossmapReference[i][0], g_nativeCrossmapReference[i][1]);
			g_source = "built-in reference table";
		}
		else
		{
			std::string path = CGlobals::Get().orangePath + "\\natives-" + GameOffsets::GameVersion() + ".txt";
			std::ifstream in(path, std::ios::binary);
			if (in.good())
			{
				std::stringstream ss;
				ss << in.rdbuf();
				g_crossmap.Parse(ss.str());
				for (size_t i = 0; i < g_crossmap.errors.size(); ++i)
					log_error << "natives crossmap " << g_crossmap.errors[i] << std::endl;
				g_source = path;
			}
			else
				log_error << "Natives: no crossmap for game version " << GameOffsets::GameVersion() << " (" << path
					<< " not found); natives are looked up by their canonical hash, which this build does not register" << std::endl;
		}
		log_info << "Natives: " << g_crossmap.Size() << " translation(s) from " << g_source << std::endl;
	}

	bool Initialized() { return g_initialized; }
	bool ObfuscatedLayout() { return g_obfuscated; }
	size_t TranslationCount() { return g_crossmap.Size(); }
	const std::string& TranslationSource() { return g_source; }

	bool Translate(uint64_t canonical, uint64_t& out)
	{
		out = canonical;
		return g_crossmap.Translate(canonical, out);
	}

	ScriptEngine::NativeHandler Lookup(uint64_t canonical)
	{
		if (!g_initialized)
			Initialize();
		uint64_t hash = canonical;
		if (!g_crossmap.Translate(canonical, hash) && g_crossmap.Size() && g_reported.insert(canonical).second)
			log_error << "Natives: no translation for 0x" << std::hex << std::uppercase << canonical << std::dec
				<< " in " << g_source << ", using the canonical hash as it is" << std::endl;
		void** table = ResolveTable();
		if (!table)
			return nullptr;
		return g_obfuscated ? FindObf(table, hash) : FindPlain(table, hash);
	}

	size_t DumpRegistered(const std::string& path)
	{
		if (!g_initialized)
			Initialize();
		void** table = ResolveTable();
		if (!table)
		{
			log_error << "Natives: RegistrationTable is unresolved, cannot dump the registered natives" << std::endl;
			return 0;
		}
		const size_t capacity = 16384;
		std::vector<uint64_t> hashes(capacity), handlers(capacity);
		size_t count = WalkAll(table, g_obfuscated, hashes.data(), handlers.data(), capacity);
		if (!count)
		{
			log_error << "Natives: the registration table walk found nothing - the table at "
				<< orange::HexString((uintptr_t)table) << " is empty. Either the game had not registered its natives yet, "
				"or the RegistrationTable offset / the obfuscated layout is wrong for this build" << std::endl;
			return 0;
		}
		uintptr_t base = (uintptr_t)GetModuleHandle(NULL);
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out.good())
		{
			log_error << "Natives: cannot write " << path << std::endl;
			return 0;
		}
		out << "; natives registered by GTA5.exe " << GameOffsets::GameVersion() << " (" << (g_obfuscated ? "obfuscated" : "plain")
			<< " registration table), written by orange-core\n";
		out << "; <hash of this build> <handler RVA>   - pair the hashes with canonical ones to build natives-"
			<< GameOffsets::GameVersion() << ".txt (docs/PORTING_STATUS.md)\n";
		for (size_t i = 0; i < count; ++i)
			out << orange::HexString(hashes[i]) << " " << orange::HexString(handlers[i] >= base ? handlers[i] - base : handlers[i]) << "\n";
		log_info << "Natives: " << count << " registered native(s) written to " << path << std::endl;
		return count;
	}
}
