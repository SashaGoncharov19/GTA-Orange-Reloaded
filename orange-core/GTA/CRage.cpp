#include "stdafx.h"

PedFactoryHook* PedFactoryHook::singleInstance = nullptr;
VehicleFactoryHook* VehicleFactoryHook::singleInstance = nullptr;
bool SyncTree::initialized = false;
GetSyncTree_ SyncTree::GetSyncTree;


namespace rageGlobals
{
	void SetPlayerColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
		LPVOID colorGlobal = GameMem("PlayerColor").getOffset(2);
		if (!colorGlobal)
			return;
		unsigned char * colorAddress = (unsigned char *)((ULONGLONG)colorGlobal + 4);
		for (int i = 0; i < 4; ++i)
		{
			(*colorAddress++) = b;
			(*colorAddress++) = g;
			(*colorAddress++) = r;
			(*colorAddress++) = a;
		}
	}
};

namespace GTA
{
	std::string CTask::GetTree(CTask *task, int n)
	{
		if (!n)
		{
			task = this;
			return VTasks::Get()->GetTaskName(this->GetID()) + GetTree(task->SubTask, n + 1);
		}
		else
		{
			std::string res("\n");
			if (!task)
				return res;
			for (int i = 0; i < n; ++i)
				res += "-";
			res += " ";
			res += VTasks::Get()->GetTaskName(task->GetID());
			return res + GetTree(task->SubTask, n + 1);
		}
	}

	CViewportGame *CViewportGame::Get()
	{
		LPVOID viewport = GameMem("ViewportGame").getOffset();
		return viewport ? *(CViewportGame**)viewport : nullptr;
	}

};

GTA::CTask * CTaskTree::GetTaskByID(unsigned int taskID)
{
	for (GTA::CTask *task = GetTask(); task; task = task->SubTask)
		if (task->GetID() == taskID)
			return task;
	return nullptr;
}

CPed * CPed::GetFromScriptID(int Handle)
{
	typedef CPed*(*GetCEntity)(int);
	static GetCEntity getEntity = GameFunc<GetCEntity>("GetEntityFromScriptHandle");
	return getEntity ? (CPed*)getEntity(Handle) : nullptr;
}

CWorld *CWorld::Get()
{
	LPVOID world = GameMem("World").getOffset();
	return world ? *(CWorld**)world : nullptr;
}

CVehicleFactory* CVehicleFactory::Get()
{
	return (CVehicleFactory*)GameMem("VehicleFactory").getOffset();
}