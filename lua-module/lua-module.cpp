// lua-module.cpp: определяет экспортированные функции для приложения DLL.
//

#include "stdafx.h"

#define EXPORT ORANGE_EXPORT

API * API::instance = nullptr;
Player players[256];

extern "C"
{
	EXPORT bool Validate(API * api)
	{
		API::Set(api);
		return true;
	}

	EXPORT const char* OnResourceTypeRegister()
	{
		return "lua";
	}

	EXPORT bool OnResourceLoad(const char* resource)
	{
		SResource::Get()->Start(resource);
		return true;
	}

	EXPORT void OnModuleInit()
	{
		SResource::Get()->Init();
		API::Get().Print("Lua module loaded");
	}

	EXPORT bool OnTick()
	{
		return SResource::Get()->OnTick();
	}

	EXPORT bool OnPlayerConnect(long playerid)
	{
		players[playerid].exists = true;
		return true; //SResource::Get()->OnPlayerConnect(playerid);
	}

	EXPORT char* OnHTTPRequest(const char* method, const char* url, const char* query, std::string body)
	{
		return SResource::Get()->OnHTTPRequest(method, url, query, body.c_str());;
	}

	EXPORT bool OnServerCommand(std::string command)
	{
		return true;
	}

	EXPORT bool OnPlayerDisconnect(long playerid, int reason)
	{
		players[playerid].exists = false;
		return true;
	}

	EXPORT bool OnPlayerUpdate(long playerid)
	{

		return true;
	}

	EXPORT bool OnPlayerCommand(long playerid, const char * command)
	{
		return SResource::Get()->OnPlayerCommand(playerid, command);
	}

	EXPORT bool OnPlayerText(long playerid, const char * text)
	{
		return true;
	}

	EXPORT bool OnKeyStateChanged(long playerid, int keycode, bool isUp)
	{
		return SResource::Get()->OnKeyStateChanged(playerid, keycode, isUp);
	}

	EXPORT void OnEvent(const char* e, std::vector<MValue> *args)
	{
		SResource::Get()->OnEvent(e, args);
	}
}

