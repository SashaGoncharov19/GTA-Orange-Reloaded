#pragma once
#include "ModuleAPI.h"

// Module-side view of the server API. The server hands us a pointer to its
// own API object in Validate(); we only ever call it through the APIBase
// vtable, so the concrete class here just stores that pointer.
class API :
	public APIBase
{
public:
	static API * instance;
	static void Set(API * api) { instance = api; }
	static API& Get() { return *instance; }
};
