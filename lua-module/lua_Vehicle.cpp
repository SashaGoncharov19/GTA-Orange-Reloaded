#include "stdafx.h"

// Vehicle functions exposed to Lua. Vehicle ids are the server GUIDs
// CreateVehicle returns.

int lua_CreateVehicle(lua_State *L)
{
	if(lua_type(L, 1) == LUA_TSTRING)
		lua_pushnumber(L, API::Get().CreateVehicle(API::Get().Hash(lua_tostring(L, 1)), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5)));
	else
		lua_pushnumber(L, API::Get().CreateVehicle(lua_tointeger(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5)));
	
	return 1;
}

int lua_DeleteVehicle(lua_State *L)
{
	lua_pushboolean(L, API::Get().DeleteVehicle((unsigned long)lua_tonumber(L, 1)));
	return 1;
}

int lua_VehicleExists(lua_State *L)
{
	lua_pushboolean(L, API::Get().VehicleExists((unsigned long)lua_tonumber(L, 1)));
	return 1;
}

int lua_GetVehicleCoords(lua_State *L)
{
	CVector3 pos = API::Get().GetVehiclePosition((unsigned long)lua_tonumber(L, 1));
	lua_pushnumber(L, pos.fX);
	lua_pushnumber(L, pos.fY);
	lua_pushnumber(L, pos.fZ);
	return 3;
}

int lua_SetVehicleCoords(lua_State *L)
{
	lua_pushboolean(L, API::Get().SetVehiclePosition((unsigned long)lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4)));
	return 1;
}

int lua_GetVehicleRotation(lua_State *L)
{
	CVector3 rot = API::Get().GetVehicleRotation((unsigned long)lua_tonumber(L, 1));
	lua_pushnumber(L, rot.fX);
	lua_pushnumber(L, rot.fY);
	lua_pushnumber(L, rot.fZ);
	return 3;
}

int lua_GetVehicleHealth(lua_State *L)
{
	lua_pushnumber(L, API::Get().GetVehicleHealth((unsigned long)lua_tonumber(L, 1)));
	return 1;
}

int lua_GetVehicleModel(lua_State *L)
{
	lua_pushinteger(L, API::Get().GetVehicleModel((unsigned long)lua_tonumber(L, 1)));
	return 1;
}

// GetVehicleDriver(vehicle) -> player id or nil
int lua_GetVehicleDriver(lua_State *L)
{
	long driver = API::Get().GetVehicleDriver((unsigned long)lua_tonumber(L, 1));
	if (driver < 0)
		lua_pushnil(L);
	else
		lua_pushinteger(L, driver);
	return 1;
}

// GetVehicles() -> array of vehicle ids
int lua_GetVehicles(lua_State *L)
{
	std::vector<unsigned long> ids = API::Get().GetVehicles();
	lua_createtable(L, (int)ids.size(), 0);
	for (size_t i = 0; i < ids.size(); i++)
	{
		lua_pushnumber(L, (lua_Number)ids[i]);
		lua_rawseti(L, -2, (int)i + 1);
	}
	return 1;
}
