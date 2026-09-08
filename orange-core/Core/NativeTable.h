#pragma once
// NativeTable: looks natives up in the game's registration table by their
// canonical hash. Natives.h calls every native by the canonical hash (the one
// natives databases use); the running game only knows the hash of its own
// build, so the lookup translates first:
//
//   reference build (January 2017)  -> built-in table, NativeCrossmap_Reference.h
//   any other build                 -> natives-<game version>.txt next to
//                                      orange-core.dll (shared/NativeCrossmap.h
//                                      describes the format)
//
// Builds from 1.0.1290 on keep the registration table obfuscated (pointers,
// counts and hashes XOR-ed with their own address); both layouts are handled.
namespace NativeTable
{
	// Loads the crossmap for the running build. Safe to call more than once.
	void Initialize();
	bool Initialized();

	// Canonical hash -> hash of the running build. Returns false (and leaves
	// `out` = canonical) when no translation is known.
	bool Translate(uint64_t canonical, uint64_t& out);

	// Handler of a native given its canonical hash, NULL when unknown.
	ScriptEngine::NativeHandler Lookup(uint64_t canonical);

	// True when the game uses the obfuscated registration layout.
	bool ObfuscatedLayout();

	// Writes every registered native of the running build as
	// "0x<build hash> 0x<handler RVA>" lines to `path` (for building a
	// crossmap). Returns the number of natives written, 0 on failure.
	size_t DumpRegistered(const std::string& path);

	size_t TranslationCount();
	const std::string& TranslationSource();
}
