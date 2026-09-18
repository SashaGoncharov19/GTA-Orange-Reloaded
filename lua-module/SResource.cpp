#include "stdafx.h"

std::stringbuf _code_;
size_t _size = 0;

static int writer(lua_State *L, const void *p, size_t size, void *u) {

	unsigned int i = 0;

	unsigned char *d = (unsigned char *)p;

	_code_.sputn((char*)d, size);
	_size += size;

	return 0;
}


void compile(lua_State *L, const char *file) {
	
	if (luaL_loadfile(L, file) != 0) {
		printf("%s\n", lua_tostring(L, -1));
	}

	lua_dump(L, writer, NULL);
	lua_pop(L, 1);
}

SResource *SResource::singleInstance = nullptr;

static const struct luaL_Reg gfunclib[] = {
	{ "print", lua_print },
	/*{ "__invoke", lua_invoke },
	{ "_i", lua_getvalue<Type::N_INT> },
	{ "_ip", lua_getvalue<Type::N_INTPOINTER> },*/
	{ NULL, NULL }
};

static const struct luaL_Reg mfunclib[] = {
	{ "CreateBlipForAll", lua_CreateBlipForAll },
	{ "CreateBlipForPlayer", lua_CreateBlipForPlayer },
	{ "DeleteBlip", lua_DeleteBlip },
	{ "SetBlipColor", lua_SetBlipColor },
	{ "SetBlipRoute", lua_SetBlipRoute },

	{ "CreateMarkerForAll", lua_CreateMarkerForAll },
	{ "CreateMarkerForPlayer", lua_CreateMarkerForPlayer },
	{ "DeleteMarker", lua_DeleteMarker },

	{ "CreateVehicle", lua_CreateVehicle },
	{ "DeleteVehicle", lua_DeleteVehicle },
	{ "VehicleExists", lua_VehicleExists },
	{ "GetVehicleCoords", lua_GetVehicleCoords },
	{ "SetVehicleCoords", lua_SetVehicleCoords },
	{ "GetVehicleRotation", lua_GetVehicleRotation },
	{ "GetVehicleHealth", lua_GetVehicleHealth },
	{ "GetVehicleModel", lua_GetVehicleModel },
	{ "GetVehicleDriver", lua_GetVehicleDriver },
	{ "GetVehicles", lua_GetVehicles },

	{ "CreateObject", lua_CreateObject },

	{ "GetPlayerCoords", lua_GetPlayerCoords },
	{ "SetPlayerCoords", lua_SetPlayerCoords },
	{ "GetPlayerName", lua_GetPlayerName },
	{ "GetPlayerModel", lua_GetPlayerModel },
	{ "GivePlayerWeapon", lua_GivePlayerWeapon },
	{ "PlayerExists", lua_PlayerExists },
	{ "SendPlayerNotification", lua_SendPlayerNotification },
	{ "SetPlayerInfoMsg", lua_SetPlayerInfoMsg },
	{ "SendPlayerMessage", lua_SendPlayerMessage },
	{ "SendMessageToAll", lua_SendMessageToAll },
	{ "SetPlayerIntoVehicle", lua_SetPlayerIntoVehicle },
	{ "SetPlayerModel", lua_SetPlayerModel },
	{ "SetPlayerName", lua_SetPlayerName },
	{ "GivePlayerAmmo", lua_GivePlayerAmmo },
	{ "GetPlayerHealth", lua_GetPlayerHealth },
	{ "SetPlayerHealth", lua_SetPlayerHealth },
	{ "GetPlayerArmour", lua_GetPlayerArmour },
	{ "SetPlayerArmour", lua_SetPlayerArmour },
	{ "GetPlayerHeading", lua_GetPlayerHeading },
	{ "SetPlayerHeading", lua_SetPlayerHeading },
	{ "GetPlayerWeapon", lua_GetPlayerWeapon },
	{ "IsPlayerInVehicle", lua_IsPlayerInVehicle },
	{ "GetPlayerVehicle", lua_GetPlayerVehicle },
	{ "GetPlayerSeat", lua_GetPlayerSeat },
	{ "GetPlayerPing", lua_GetPlayerPing },
	{ "GetPlayerAddress", lua_GetPlayerAddress },
	{ "GetPlayerClientVersion", lua_GetPlayerClientVersion },
	{ "IsPlayerDead", lua_IsPlayerDead },
	{ "IsPlayerInRange", lua_IsPlayerInRange },
	{ "KickPlayer", lua_KickPlayer },
	{ "GetPlayerMoney", lua_GetPlayerMoney },
	{ "SetPlayerMoney", lua_SetPlayerMoney },
	{ "GivePlayerMoney", lua_GivePlayerMoney },
	{ "SetPlayerColor", lua_SetPlayerColor },
	{ "GetPlayerColor", lua_GetPlayerColor },
	{ "GetPlayers", lua_GetPlayers },
	{ "GetPlayerCount", lua_GetPlayerCount },
	{ "GetMaxPlayers", lua_GetMaxPlayers },
	
	{ "AddClientScript", lua_LoadClientScript },
	{ "OnTick", lua_tick },
	{ "SetTimer", lua_SetTimer },
	{ "SetInterval", lua_SetInterval },
	{ "ClearTimer", lua_ClearTimer },
	{ "GetServerTime", lua_GetServerTime },
	{ "OnHTTPReq", lua_HTTPReq },
	{ "OnEvent", lua_Event },
	{ "OnCommand", lua_Command },
#ifndef _LUA_NOSQL
	{ "SQLEnv", luaopen_luasql_mysql },
#endif

	{ "Create3DText", lua_Create3DText },
	{ "Set3DTextText", lua_Set3DTextText },
	{ "Attach3DTextToVeh", lua_Attach3DTextToVeh },
	{ "Attach3DTextToPlayer", lua_Attach3DTextToPlayer },
	{ "Delete3DText", lua_Delete3DText },

	{ NULL, NULL }
};

SResource *SResource::Get()
{
	if (!singleInstance) {
		singleInstance = new SResource();
	}
	return singleInstance;
}
SResource::SResource()
{
}

bool SResource::Init()
{
	m_lua = luaL_newstate();
	luaJIT_setmode(m_lua, 0, LUAJIT_MODE_ENGINE | true);

	luaL_openlibs(m_lua);

	lua_getglobal(m_lua, "_G");
	luaL_setfuncs(m_lua, gfunclib, 0);
	lua_pop(m_lua, 1);

	lua_newtable(m_lua);
	luaL_setfuncs(m_lua, mfunclib, 0);
	lua_setglobal(m_lua, "__orange__");

	if (luaL_loadfile(m_lua, "modules//lua-module//API.lua") || lua_pcall(m_lua, 0, 0, 0)) {
		std::stringstream ss;
		ss << "[LUA] " << lua_tostring(m_lua, -1);
		API::Get().Print(ss.str().c_str());
		return false;
	}

	/*if (luaL_loadbuffer(m_lua, luaJIT_BC_main, luaJIT_BC_main_SIZE, NULL) || lua_pcall(m_lua, 0, 0, 0)) {
		std::stringstream ss;
		ss << "[LUA] " << lua_tostring(m_lua, -1);
		API::Get().Print(ss.str().c_str());
		return false;
	}*/

	//std::cout << /*lua_dump(m_lua)*/  << std::endl;

	return true;
}

void SResource::AddClientScript(std::string file)
{
	compile(m_lua, file.c_str());

	char* _code = new char[_size];
	_code_.sgetn(_code, _size);

	API::Get().Print("ADD");

	/*if (luaL_loadbuffer(m_lua, _code, _size, NULL) || lua_pcall(m_lua, 0, 0, 0)) {
		std::stringstream ss;
		ss << "[LUA] " << lua_tostring(m_lua, -1);
		API::Get().Print(ss.str().c_str());
	}*/

	API::Get().LoadClientScript(file.c_str(), _code, _size);

	_size = 0;
}

bool SResource::Start(const char* name)
{
	char path[64];
	char respath[64];

	sprintf(path, "resources//%s//", name);
	sprintf(respath, "%smain.lua", path);

	std::stringstream ss;
	ss << "[LUA] Starting resource " << name;
	API::Get().Print(ss.str().c_str());	

	if (luaL_loadfile(m_lua, respath) || lua_pcall(m_lua, 0, 0, 0)) {
		std::stringstream ss;
		ss << "[LUA] " << lua_tostring(m_lua, -1);
		API::Get().Print(ss.str().c_str());
		return false;
	}

	return true;
}

char* SResource::OnHTTPRequest(const char* method, const char* url, const char* query, const char* body)
{
	if (!http)
		return NULL;
	return http(method, url, query, body);
}

bool SResource::OnTick()
{
	RunTimers();
	if (tick)
		tick();
	return true;
}

int SResource::AddTimer(int ref, unsigned long intervalMs, bool repeat)
{
	Timer t;
	t.id = m_nextTimerId++;
	t.ref = ref;
	t.intervalMs = intervalMs;
	t.dueMs = API::Get().GetServerTimeMs() + intervalMs;
	t.repeat = repeat;
	t.removed = false;
	m_timers.push_back(t);
	return t.id;
}

bool SResource::RemoveTimer(int id)
{
	for (Timer & t : m_timers)
		if (t.id == id && !t.removed)
		{
			t.removed = true;
			return true;
		}
	return false;
}

void SResource::RunTimers()
{
	if (m_timers.empty())
		return;
	unsigned long now = API::Get().GetServerTimeMs();
	// Callbacks may add or clear timers: iterate by index over the current size.
	size_t count = m_timers.size();
	for (size_t i = 0; i < count; i++)
	{
		Timer & t = m_timers[i];
		if (t.removed || (long)(now - t.dueMs) < 0)
			continue;
		if (t.repeat)
			t.dueMs = now + t.intervalMs;
		else
			t.removed = true;
		lua_rawgeti(m_lua, LUA_REGISTRYINDEX, t.ref);
		if (lua_pcall(m_lua, 0, 0, 0) != 0)
		{
			std::string err = lua_tostring(m_lua, -1) ? lua_tostring(m_lua, -1) : "unknown error";
			lua_pop(m_lua, 1);
			API::Get().Print(("[LUA] timer: " + err).c_str());
		}
	}
	// drop the finished ones and release their callbacks
	for (size_t i = 0; i < m_timers.size();)
	{
		if (m_timers[i].removed)
		{
			luaL_unref(m_lua, LUA_REGISTRYINDEX, m_timers[i].ref);
			m_timers.erase(m_timers.begin() + i);
		}
		else
			i++;
	}
}

bool SResource::OnPlayerCommand(long playerid, const char* cmd)
{
	if (!oncommand)
		return true;
	return oncommand(playerid, cmd);
}

void SResource::SetHTTP(const std::function<char*(const char* method, const char* url, const char* query, const char* body)>& t)
{
	http = t;
}

void SResource::SetTick(const std::function<void()>& t)
{
	tick = t;
}

void SResource::SetEvent(const std::function<void(const char* e, std::vector<MValue> *args)>& t)
{
	onevent = t;
}

void SResource::SetCommandProcessor(const std::function<bool(long pid, const char*command)>& t)
{
	oncommand = t;
}


bool SResource::OnKeyStateChanged(long playerid, int keycode, bool isUp)
{
	lua_getglobal(m_lua, "__OnKeyStateChanged");
	if (lua_isnil(m_lua, -1))
	{
		lua_pop(m_lua, 1);
		return true;
	}

	lua_pushinteger(m_lua, playerid);
	lua_pushinteger(m_lua, keycode);
	lua_pushboolean(m_lua, isUp);

	if (lua_pcall(m_lua, 3, 0, 0)) API::Get().Print("Error in OnKeyStateChanged callback");

	return true;
}

void SResource::OnEvent(const char* e, std::vector<MValue> *args)
{
	if (onevent)
		onevent(e, args);
}

SResource::~SResource()
{
}
