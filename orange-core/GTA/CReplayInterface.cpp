#include "stdafx.h"
ReplayInterfaces* ReplayInterfaces::Get()
{
	LPVOID interfaces = GameMem("ReplayInterfaces").getOffset();
	return interfaces ? *(ReplayInterfaces**)interfaces : nullptr;
}