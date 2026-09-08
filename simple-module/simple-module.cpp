
#include "stdafx.h"

API * API::instance = nullptr;

extern "C"
{
	ORANGE_EXPORT bool Validate(API * api)
	{
		API::Set(api);
		return true;
	}

	ORANGE_EXPORT void OnModuleInit()
	{
		API::Get().Print("Simple module has been successfully loaded!");

		API::Get().CreateVehicle(0x6322B39A, 46.34f, -688.664f, 43.6748f, 133.994f);
		API::Get().CreateVehicle(0x6322B39A, 43.0207f, -695.123f, 43.662f, 159.652f);
		API::Get().CreateVehicle(0x6322B39A, 40.2774f, -702.291f, 43.6512f, 158.812f);
		API::Get().CreateVehicle(0x6322B39A, 38.2197f, -707.548f, 43.6416f, 158.856f);

		API::Get().Print("All cars were loaded!");
	}

	ORANGE_EXPORT bool OnPlayerConnect(long playerid)
	{
		std::stringstream message;
		message << "Player " << API::Get().GetPlayerName(playerid) << " joined the server!";
		API::Get().BroadcastClientMessage(message.str().c_str(), 0xFFFFFFFF);
		API::Get().SetPlayerPosition(playerid, 21.2369f, -711.042f, 45.973f);

		API::Get().GivePlayerWeapon(playerid, 0x2BE6766B, 2000);
		API::Get().GivePlayerWeapon(playerid, 0xBFEFFF6D, 2000);
		API::Get().GivePlayerWeapon(playerid, 0x1B06D571, 2000);

		return true;
	}

	ORANGE_EXPORT bool OnServerCommand(std::string command)
	{

		return true;
	}

	ORANGE_EXPORT bool OnPlayerDisconnect(long playerid, int reason)
	{
		std::stringstream message;
		message << "Player  " << API::Get().GetPlayerName(playerid) << " left the server(" << ((reason == 1) ? "Disconnected" : "Timeout") << ")!";
		API::Get().BroadcastClientMessage(message.str().c_str(), 0xFFFFFFFF);
		return true;
	}

	ORANGE_EXPORT bool OnPlayerUpdate(long playerid)
	{

		return true;
	}

	ORANGE_EXPORT bool OnPlayerCommand(long playerid, const char * command)
	{

		return true;
	}

	ORANGE_EXPORT bool OnPlayerText(long playerid, const char * text)
	{

		return true;
	}
}

