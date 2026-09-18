#include "stdafx.h"

int lua_print(lua_State *L)
{
	std::stringstream ss;
	int nargs = lua_gettop(L);
	for (int i = 1; i <= nargs; ++i) {
		ss << lua_tostring(L, i) << "\t";
	}
	API::Get().Print(ss.str().c_str());
	return 0;
}

int lua_tick(lua_State *L)
{
	lua_pushvalue(L, 1);

	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	SResource::Get()->SetTick([=]()
	{
		lua_pushvalue(L, 1);

		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

		if (lua_pcall(L, 0, 0, 0) != 0)
		{
			std::string err = luaL_checkstring(L, -1);
			lua_pop(L, 1);

			API::Get().Print(err.c_str());
		}

		lua_pop(L, 1);
	});

	return 0;
}

int lua_HTTPReq(lua_State *L)
{
	lua_pushvalue(L, 1);

	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	SResource::Get()->SetHTTP([=](const char* method, const char* url, const char* query, std::string body)
	{
		lua_pushvalue(L, 1);

		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

		lua_pushstring(L, method);
		lua_pushstring(L, url);
		lua_pushstring(L, query);
		lua_pushstring(L, body.c_str());

		if (lua_pcall(L, 4, 1, 0)) {
			std::string err = luaL_checkstring(L, -1);
			lua_pop(L, 1);

			API::Get().Print(err.c_str());
		}
		if (lua_isnil(L, -1)) {
			lua_pop(L, 2);
			return (char*)NULL;
		}

		char* res = strdup(lua_tostring(L, -1));
		lua_pop(L, 2);

		return res;
	});

	return 0;
}

int lua_Event(lua_State *L)
{
	lua_pushvalue(L, 1);

	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	SResource::Get()->SetEvent([=](const char* e, std::vector<MValue> *args)
	{
		lua_pushvalue(L, 1);

		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

		lua_pushstring(L, e);

		int count = 1;

		for(int i = 0; i < args->size(); i++)
		{
			count++;
			MValue param = args->at(i);
			switch (param.type)
			{
			case M_BOOL:
				lua_pushboolean(L, param.getBool());
				break;
			case M_INT:
				lua_pushinteger(L, param.getInt());
				break;
			case M_DOUBLE:
				lua_pushnumber(L, param.getDouble());
				break;
			case M_ULONG:
				lua_pushinteger(L, param.getULong());
				break;
			case M_STRING:
				lua_pushstring(L, param.getString());
				break;
			}
		}

		if (lua_pcall(L, count, 0, 0)) {
			std::string err = luaL_checkstring(L, -1);
			lua_pop(L, 1);

			API::Get().Print(err.c_str());
		}

		lua_pop(L, 1);
	});

	return 0;
}

int lua_Command(lua_State *L)
{
	lua_pushvalue(L, 1);

	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	SResource::Get()->SetCommandProcessor([=](long pid, const char* cmd)
	{
		lua_pushvalue(L, 1);

		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

		lua_pushinteger(L, pid);
		lua_pushstring(L, cmd);

		if (lua_pcall(L, 2, 1, 0)) {
			std::string err = luaL_checkstring(L, -1);
			lua_pop(L, 1);

			API::Get().Print(err.c_str());
		}
		if (lua_isnil(L, -1)) {
			lua_pop(L, 2);
			return true;
		}

		bool res = lua_toboolean(L, -1);
		lua_pop(L, 2);

		return res;
	});

	return 0;
}

int lua_Create3DText(lua_State *L)
{
	lua_pushinteger(L, API::Get().Create3DText(lua_tostring(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tointeger(L, 5), lua_tointeger(L, 6), lua_tonumber(L, 7)));
	return 1;
}

int lua_Delete3DText(lua_State *L)
{
	API::Get().Delete3DText(lua_tointeger(L, 1));
	return 0;
}

int lua_Set3DTextText(lua_State *L)
{
	API::Get().Set3DTextContent(lua_tointeger(L, 1), lua_tostring(L, 2));
	return 0;
}

int lua_Attach3DTextToVeh(lua_State *L)
{
	API::Get().Attach3DTextToVehicle(lua_tointeger(L, 1), lua_tointeger(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5));
	return 0;
}

int lua_Attach3DTextToPlayer(lua_State *L)
{
	API::Get().Attach3DTextToPlayer(lua_tointeger(L, 1), lua_tointeger(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5));
	return 0;
}

int lua_LoadClientScript(lua_State *L)
{
	SResource::Get()->AddClientScript(lua_tostring(L, 1));
	return 0;
}

int lua_Delete3DTextToPlayer(lua_State *L)
{
	API::Get().Delete3DText(lua_tointeger(L, 1));
	return 0;
}


// SetTimer(ms, fn) -> id: fn runs once after ms milliseconds.
int lua_SetTimer(lua_State *L)
{
	unsigned long ms = (unsigned long)luaL_checknumber(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	lua_pushvalue(L, 2);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_pushinteger(L, SResource::Get()->AddTimer(ref, ms, false));
	return 1;
}

// SetInterval(ms, fn) -> id: fn runs every ms milliseconds until ClearTimer(id).
int lua_SetInterval(lua_State *L)
{
	unsigned long ms = (unsigned long)luaL_checknumber(L, 1);
	if (ms < 1) ms = 1;
	luaL_checktype(L, 2, LUA_TFUNCTION);
	lua_pushvalue(L, 2);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_pushinteger(L, SResource::Get()->AddTimer(ref, ms, true));
	return 1;
}

int lua_ClearTimer(lua_State *L)
{
	lua_pushboolean(L, SResource::Get()->RemoveTimer((int)luaL_checkinteger(L, 1)));
	return 1;
}

// GetServerTime() -> milliseconds since the server started (monotonic).
int lua_GetServerTime(lua_State *L)
{
	lua_pushnumber(L, (lua_Number)API::Get().GetServerTimeMs());
	return 1;
}
