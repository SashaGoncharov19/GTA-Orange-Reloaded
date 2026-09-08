// GameOffsets: the single table of every GTA5.exe address orange-core hooks,
// patches or reads. Nothing else in orange-core hard-codes an RVA any more.
//
// Resolution order for each entry (see docs/UPDATING_OFFSETS.md):
//   1. offsets.ini next to orange-core.dll, section [<game version>]
//   2. offsets.ini, section [default]
//   3. the built-in reference RVA, if GTA5.exe is the reference build
//      (the January 2017 build the original authors worked on)
//   4. the built-in byte pattern, if there is one
//   5. unresolved: optional entries are skipped, required ones keep
//      orange-core inactive (unless orange.developer exists)
#pragma once

#include <string>
#include <vector>

namespace GameOffsets
{
	enum class Source
	{
		Unresolved,
		Disabled,     // "Name = disabled" in offsets.ini
		Reference,    // built-in RVA of the reference build
		Ini,          // RVA from offsets.ini
		Scan,         // found with a byte pattern (built-in or from offsets.ini)
	};

	struct Entry
	{
		const char* name;         // key in offsets.ini, case insensitive
		uintptr_t referenceRva;   // RVA in the reference build
		const char* pattern;      // "48 8B ? ? E8" style, NULL when none is known
		int patternDelta;         // added to the pattern match
		bool required;            // orange-core cannot work without it
		const char* comment;      // what it is / what is done with it
	};

	struct Status
	{
		const Entry* entry;
		Source source;
		uintptr_t rva;            // 0 when not resolved
		std::string note;         // why it failed / ambiguity warnings
	};

	// Reads offsets.ini, detects the game build and resolves every entry.
	// Safe to call more than once. Returns AllRequiredResolved().
	bool Initialize();

	bool IsInitialized();
	bool IsReferenceBuild();
	bool AllRequiredResolved();
	std::vector<std::string> UnresolvedRequired();
	const std::vector<Status>& All();

	// Version of GTA5.exe from its version resource, e.g. "1.0.944.2".
	// "unknown" when it cannot be read.
	const std::string& GameVersion();
	// Third component of the version ("1.0.3889.0" -> 3889), 0 when unknown.
	int GameBuildNumber();

	// Absolute address of a resolved entry, 0 when unresolved or disabled.
	uintptr_t Address(const char* name);
	uintptr_t Rva(const char* name);
	bool IsResolved(const char* name);
	Source SourceOf(const char* name);
	// True when the running game is at least `build` (third version component)
	// and not the reference build; false when the version is unknown.
	bool BuildAtLeast(int build);

	// Writes offsets-<version>.generated.ini: every entry with its current
	// state, ready to be filled in for a new game build.
	bool WriteTemplate(const std::string& path);

	const char* SourceName(Source source);
}

// CMemory positioned at a named offset. When the entry is unresolved the
// address is 0 and CMemory silently skips writes (with a line in client.log),
// so optional patches degrade gracefully on unknown game builds.
inline CMemory GameMem(const char* name)
{
	return CMemory((UINT64)GameOffsets::Address(name));
}

// Function pointer / typed pointer at a named offset, NULL when unresolved.
template <typename T>
inline T GameFunc(const char* name)
{
	return (T)GameOffsets::Address(name);
}
