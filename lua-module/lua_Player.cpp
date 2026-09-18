#include "stdafx.h"

// Player functions exposed to Lua. Player ids are the server's small
// integers (0..), the same the events carry.

static unsigned int OptColor(lua_State *L, int index, unsigned int fallback)
{
	return lua_isnumber(L, index) ? (unsigned int)lua_tonumber(L, index) : fallback;
}

int lua_GetPlayerCoords(lua_State *L)
{
	CVector3 pos = API::Get().GetPlayerPosition(lua_tointeger(L, 1));

	lua_pushnumber(L, pos.fX);
	lua_pushnumber(L, pos.fY);
	lua_pushnumber(L, pos.fZ);

	return 3;
}

int lua_SetPlayerCoords(lua_State *L)
{
	lua_pushboolean(L, API::Get().SetPlayerPosition(lua_tointeger(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4)));
	return 1;
}

int lua_GetPlayerModel(lua_State *L)
{
	lua_pushinteger(L, API::Get().GetPlayerModel(lua_tointeger(L, 1)));
	return 1;
}

int lua_SetPlayerModel(lua_State *L)
{
	long model = lua_type(L, 2) == LUA_TSTRING ? API::Get().Hash(lua_tostring(L, 2)) : lua_tointeger(L, 2);
	lua_pushboolean(L, API::Get().SetPlayerModel(lua_tointeger(L, 1), model));
	return 1;
}

int lua_GetPlayerName(lua_State *L)
{
	std::string name = API::Get().GetPlayerName(lua_tointeger(L, 1));

	lua_pushstring(L, name.c_str());

	return 1;
}

int lua_SetPlayerName(lua_State *L)
{
	lua_pushboolean(L, API::Get().SetPlayerName(lua_tointeger(L, 1), luaL_checkstring(L, 2)));
	return 1;
}

int lua_SetPlayerInfoMsg(lua_State *L)
{
	if(lua_toboolean(L, 2))	API::Get().SetInfoMsg(lua_tointeger(L, 1), lua_tostring(L, 2));
	else API::Get().UnsetInfoMsg(lua_tointeger(L, 1));

	return 0;
}

int lua_SendPlayerNotification(lua_State *L)
{
	API::Get().SendNotification(lua_tointeger(L, 1), lua_tostring(L, 2));

	return 0;
}

// SendPlayerMessage(playerid, text [, color])
int lua_SendPlayerMessage(lua_State *L)
{
	API::Get().SendClientMessage(lua_tointeger(L, 1), luaL_checkstring(L, 2), OptColor(L, 3, 0xFFFFFFFF));

	return 0;
}

// SendMessageToAll(text [, color])
int lua_SendMessageToAll(lua_State *L)
{
	API::Get().BroadcastClientMessage(luaL_checkstring(L, 1), OptColor(L, 2, 0xFFFFFFFF));
	return 0;
}

int lua_PlayerExists(lua_State *L)
{
	lua_pushboolean(L, API::Get().PlayerExists(lua_tointeger(L, 1)));

	return 1;
}

int lua_GivePlayerWeapon(lua_State *L)
{
	if (lua_type(L, 2) == LUA_TSTRING)
		lua_pushboolean(L, API::Get().GivePlayerWeapon(lua_tointeger(L, 1), API::Get().Hash(lua_tostring(L, 2)), lua_tointeger(L, 3)));
	else
		lua_pushboolean(L, API::Get().GivePlayerWeapon(lua_tointeger(L, 1), lua_tointeger(L, 2), lua_tointeger(L, 3)));

	return 1;
}

int lua_GivePlayerAmmo(lua_State *L)
{
	long weapon = lua_type(L, 2) == LUA_TSTRING ? API::Get().Hash(lua_tostring(L, 2)) : lua_tointeger(L, 2);
	lua_pushboolean(L, API::Get().GivePlayerAmmo(lua_tointeger(L, 1), weapon, lua_tointeger(L, 3)));
	return 1;
}

int lua_SetPlayerIntoVehicle(lua_State *L)
{
	API::Get().SetPlayerIntoVehicle(lua_tointeger(L, 1), lua_tointeger(L, 2), lua_tointeger(L, 3));
	return 0;
}

int lua_GetPlayerHealth(lua_State *L)
{
	lua_pushnumber(L, API::Get().GetPlayerHealth(lua_tointeger(L, 1)));
	return 1;
}

int lua_SetPlayerHealth(lua_State *L)
{
	lua_pushboolean(L, API::Get().SetPlayerHealth(lua_tointeger(L, 1), (float)lua_tonumber(L, 2)));
	return 1;
}

int lua_GetPlayerArmour(lua_State *L)
{
	lua_pushnumber(L, API::Get().GetPlayerArmour(lua_tointeger(L, 1)));
	return 1;
}

int lua_SetPlayerArmour(lua_State *L)
{
	lua_pushboolean(L, API::Get().SetPlayerArmour(lua_tointeger(L, 1), (float)lua_tonumber(L, 2)));
	return 1;
}

int lua_GetPlayerHeading(lua_State *L)
{
	lua_pushnumber(L, API::Get().GetPlayerHeading(lua_tointeger(L, 1)));
	return 1;
}

int lua_SetPlayerHeading(lua_State *L)
{
	lua_pushboolean(L, API::Get().SetPlayerHeading(lua_tointeger(L, 1), (float)lua_tonumber(L, 2)));
	return 1;
}

int lua_GetPlayerWeapon(lua_State *L)
{
	lua_pushinteger(L, API::Get().GetPlayerWeapon(lua_tointeger(L, 1)));
	return 1;
}

int lua_IsPlayerInVehicle(lua_State *L)
{
	lua_pushboolean(L, API::Get().IsPlayerInVehicle(lua_tointeger(L, 1)));
	return 1;
}

int lua_GetPlayerVehicle(lua_State *L)
{
	unsigned long veh = API::Get().GetPlayerVehicle(lua_tointeger(L, 1));
	if (veh)
		lua_pushnumber(L, (lua_Number)veh);
	else
		lua_pushnil(L);
	return 1;
}

int lua_GetPlayerSeat(lua_State *L)
{
	lua_pushinteger(L, API::Get().GetPlayerSeat(lua_tointeger(L, 1)));
	return 1;
}

int lua_GetPlayerPing(lua_State *L)
{
	lua_pushinteger(L, API::Get().GetPlayerPing(lua_tointeger(L, 1)));
	return 1;
}

int lua_GetPlayerAddress(lua_State *L)
{
	lua_pushstring(L, API::Get().GetPlayerAddress(lua_tointeger(L, 1)).c_str());
	return 1;
}

int lua_GetPlayerClientVersion(lua_State *L)
{
	lua_pushstring(L, API::Get().GetPlayerClientVersion(lua_tointeger(L, 1)).c_str());
	return 1;
}

int lua_IsPlayerDead(lua_State *L)
{
	lua_pushboolean(L, API::Get().IsPlayerDead(lua_tointeger(L, 1)));
	return 1;
}

int lua_IsPlayerInRange(lua_State *L)
{
	lua_pushboolean(L, API::Get().IsPlayerInRange(lua_tointeger(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5)));
	return 1;
}

int lua_KickPlayer(lua_State *L)
{
	lua_pushboolean(L, API::Get().KickPlayer(lua_tointeger(L, 1), lua_isstring(L, 2) ? lua_tostring(L, 2) : "kicked"));
	return 1;
}

int lua_GetPlayerMoney(lua_State *L)
{
	lua_pushnumber(L, (lua_Number)API::Get().GetPlayerMoney(lua_tointeger(L, 1)));
	return 1;
}

int lua_SetPlayerMoney(lua_State *L)
{
	lua_pushboolean(L, API::Get().SetPlayerMoney(lua_tointeger(L, 1), lua_tointeger(L, 2)));
	return 1;
}

int lua_GivePlayerMoney(lua_State *L)
{
	lua_pushboolean(L, API::Get().GivePlayerMoney(lua_tointeger(L, 1), lua_tointeger(L, 2)));
	return 1;
}

int lua_SetPlayerColor(lua_State *L)
{
	lua_pushboolean(L, API::Get().SetPlayerColor(lua_tointeger(L, 1), (unsigned int)lua_tonumber(L, 2)));
	return 1;
}

int lua_GetPlayerColor(lua_State *L)
{
	lua_pushnumber(L, (lua_Number)API::Get().GetPlayerColor(lua_tointeger(L, 1)));
	return 1;
}

// GetPlayers() -> array of player ids
int lua_GetPlayers(lua_State *L)
{
	std::vector<long> ids = API::Get().GetPlayers();
	lua_createtable(L, (int)ids.size(), 0);
	for (size_t i = 0; i < ids.size(); i++)
	{
		lua_pushinteger(L, ids[i]);
		lua_rawseti(L, -2, (int)i + 1);
	}
	return 1;
}

int lua_GetPlayerCount(lua_State *L)
{
	lua_pushinteger(L, API::Get().GetPlayerCount());
	return 1;
}

int lua_GetMaxPlayers(lua_State *L)
{
	lua_pushinteger(L, API::Get().GetMaxPlayers());
	return 1;
}
