
#pragma once

#ifdef _WIN32
#include "targetver.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#endif

#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <functional>
#include "API.h"

#ifndef _LUA_NOSQL
#include "mysql.h"
#endif

#include "lua.hpp"
#include "lua_Main.h"

#include "SResource.h"

// Lua 5.2 style helper that LuaJIT 2.1.0-beta2 does not provide.
static void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup)
{
	luaL_checkstack(L, nup + 1, "too many upvalues");
	for (; l->name != NULL; l++) {  /* fill the table with given functions */
		int i;
		lua_pushstring(L, l->name);
		for (i = 0; i < nup; i++)  /* copy upvalues to the top */
			lua_pushvalue(L, -(nup + 1));
		lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
		lua_settable(L, -(nup + 3));
	}
	lua_pop(L, nup);  /* remove upvalues */
};

struct Player
{
	bool exists = false;
	CVector3 pos;
};

extern Player players[256];
