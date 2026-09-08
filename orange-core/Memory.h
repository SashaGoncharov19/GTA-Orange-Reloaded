#pragma once

// Small helper for reading and patching game memory.
//
// A CMemory whose address is 0 (an unresolved GameOffsets entry) is inert:
// every write is skipped with a line in client.log and every read returns
// NULL, so optional patches degrade gracefully on unknown game builds.
class CMemory
{
	void *address;
	static bool memoryCompare(const BYTE *data, const BYTE *pattern, size_t length);
	static bool memoryCompare(const BYTE *data, const BYTE *pattern, const char *mask);
	// Logs and returns false when the address is unresolved.
	bool writable(size_t length) const;
public:
	template<typename T, T Value>
	static T Return()
	{
		return Value;
	}

	CMemory(UINT64 address);
	~CMemory();

	bool valid() const { return address != nullptr; }

	void put(const char *value);
	template <typename T> void put(T value);
	template <typename T> void put(void *address, T value);
	template <typename T> void jump(T func);
	template <typename T> void call(T func);
	void farJmp(LPVOID func);
	void farCall(LPVOID func);
	void nearCall(DWORD offset);
	void retn();
	template <typename T> T* get(int offset);
	template <typename T> T get_call();

	// Resolves the rel32 displacement at `offset` (3 for "48 8B 05 rel32",
	// 2 for "89 15 rel32", ...) to the absolute address it points to.
	LPVOID getOffset(int offset = 3)
	{
		if (!address)
			return nullptr;
		long* ptr = (long*)((intptr_t)address + offset);
		return (LPVOID)(*ptr + ((intptr_t)address + offset + sizeof(long)));
	}

	DWORD getFunc()
	{
		return (DWORD)((uintptr_t)address);
	}

	CMemory operator+(uintptr_t offset) const
	{
		return CMemory(address ? (uintptr_t)(this->address) + offset : 0);
	}

	CMemory operator-(uintptr_t offset) const
	{
		return CMemory(address ? (uintptr_t)(this->address) - offset : 0);
	}

	uintptr_t operator()() const
	{
		return (uintptr_t)this->address;
	}

	void nop(size_t length);

	static CMemory& Find(const char* pattern);
	static bool Check(std::string search, std::string mask, UINT64 offset);
};

template<typename T>
inline void CMemory::put(T value)
{
	if (!writable(sizeof(T)))
		return;
	DWORD dwProtectOld, back;
	VirtualProtect((LPVOID)address, sizeof(T), PAGE_EXECUTE_READWRITE, &dwProtectOld);
	memcpy(address, &value, sizeof(T));
	VirtualProtect((LPVOID)address, sizeof(T), dwProtectOld, &back);
	address = (LPVOID)((uint64_t)address + sizeof(T));
}

template<typename T>
inline void CMemory::put(void * address, T value)
{
	if (!address)
	{
		log_error << "CMemory: write of " << sizeof(T) << " byte(s) to a NULL address skipped" << std::endl;
		return;
	}
	DWORD dwProtectOld, back;
	VirtualProtect((LPVOID)address, sizeof(T), PAGE_EXECUTE_READWRITE, &dwProtectOld);
	memcpy(address, &value, sizeof(T));
	VirtualProtect((LPVOID)address, sizeof(T), dwProtectOld, &back);
}

template<typename T>
inline void CMemory::jump(T func)
{
	if (!writable(5))
		return;
	put(address, (uint8_t)0xE9);
	put((void*)((size_t*)address + 1), (intptr_t)func - (intptr_t)baseDiff - 5);
}

template<typename T>
inline void CMemory::call(T func)
{
	if (!writable(5))
		return;
	put(address, (uint8_t)0xE8);
	put((void*)((uintptr_t)address + 1), DWORD((intptr_t)func - (intptr_t)baseDiff - 5));
}

inline void CMemory::retn()
{
	if (!writable(1))
		return;
	put(address, (uint8_t)0xC3);
}

template<typename T>
inline T* CMemory::get(int offset)
{
	if (!address)
		return nullptr;
	char* ptr = reinterpret_cast<char*>(address);
	return reinterpret_cast<T*>(ptr + offset);
}

template<typename T>
inline T CMemory::get_call()
{
	if (!address)
		return (T)0;
	intptr_t target = *(long*)(intptr_t(address) + 1);
	target += (intptr_t(address) + 5);
	return (T)target;
}
