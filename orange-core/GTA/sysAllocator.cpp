#include "stdafx.h"

namespace rage
{
	sysMemAllocator::sysMemAllocator()
	{
		heaps = new int64_t[HEAP_LAST];
		memset(heaps, 0, sizeof(int64_t) * HEAP_LAST);
		int64_t* taskHeap = (int64_t*)GameMem("HeapTask").getOffset();
		int64_t* taskCloneHeap = (int64_t*)GameMem("HeapTaskClone").getOffset();
		if (taskHeap)
			heaps[HEAP_TASK] = *taskHeap;
		if (taskCloneHeap)
			heaps[HEAP_TASK_CLONE] = *taskCloneHeap;
		if (!taskHeap || !taskCloneHeap)
			log_error << "sysMemAllocator: task heap offsets unresolved, task synchronisation disabled" << std::endl;
	}
	sysMemAllocator* sysMemAllocator::singleInstance = nullptr;
	void* sysUseAllocator::operator new(size_t size)
	{
		return sysMemAllocator::Get()->allocate(size, 16, 0);
	}

	void sysUseAllocator::operator delete(void* memory)
	{
		sysMemAllocator::Get()->free(memory);
	}

	void* sysMemAllocator::allocate(int64_t size, int64_t align, int heapNumber, int64_t suballocator)
	{
		typedef void*(*__func)(int64_t, int64_t, int64_t, int64_t);
		static __func allocate_ = GameFunc<__func>("SysAllocate");
		if (!allocate_ || !heaps[heapNumber])
			return nullptr;
		return allocate_(heaps[heapNumber], size, align, suballocator);
	}

	void sysMemAllocator::free(void* address, int heapNumber)
	{
		typedef void(*__func)(int64_t, void*);
		static __func free_ = GameFunc<__func>("SysFree");
		if (!free_ || !heaps[heapNumber] || !address)
			return;
		free_(heaps[heapNumber], address);
	}
}

