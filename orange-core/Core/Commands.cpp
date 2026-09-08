#include "stdafx.h"

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
}

int CommandProcessor(std::string command)
{
	std::string fullCmd = command;
	std::vector<std::string> params = split(command, ' ');
	command = params[0];
	std::transform(command.begin(), command.end(), command.begin(), ::tolower);
	params.erase(params.begin());
	if (!command.compare("/quit") || !command.compare("/q"))
	{
		ExitProcess(EXIT_SUCCESS);
		return true;
	}

	// /connect [host[:port]] [port] - connects to a server (the address is
	// remembered in config.xml); without arguments to the remembered one.
	if (!command.compare("/connect"))
	{
		std::string host = CGlobals::Get().serverIP;
		int port = CGlobals::Get().serverPort;
		std::vector<std::string> args;
		for (auto & p : params)
			if (!p.empty())
				args.push_back(p);
		if (args.size() >= 1)
		{
			host = args[0];
			size_t colon = host.rfind(':');
			if (colon != std::string::npos)
			{
				port = std::atoi(host.substr(colon + 1).c_str());
				host = host.substr(0, colon);
			}
		}
		if (args.size() >= 2)
			port = std::atoi(args[1].c_str());
		if (host.empty() || host.size() >= sizeof(CGlobals::Get().serverIP) || port < 1 || port > 65535)
		{
			CChat::Get()->AddChatMessage("USAGE: /connect [host[:port]] [port]   (remembered: " + std::string(CGlobals::Get().serverIP) + ":" + std::to_string(CGlobals::Get().serverPort) + ")", 0xAAAAAAFF);
			return true;
		}
		strncpy_s(CGlobals::Get().serverIP, sizeof(CGlobals::Get().serverIP), host.c_str(), _TRUNCATE);
		CGlobals::Get().serverPort = port;
		CConfig::Get()->sNickName = std::string(CGlobals::Get().nickName);
		CConfig::Get()->sIP = host;
		CConfig::Get()->uiPort = (unsigned int)port;
		CConfig::Get()->Save();
		CGlobals::Get().displayServerBrowser = false;
		CGlobals::Get().showChat = true;
		CScriptInvoker::Get().Push([=]() {
			CNetworkConnection::Get()->ConnectTo(host, (unsigned short)port);
		});
		return true;
	}
	if (!command.compare("/disconnect"))
	{
		CScriptInvoker::Get().Push([]() {
			if (CNetworkConnection::Get()->IsConnected())
				CNetworkConnection::Get()->Disconnect();
			else
				CChat::Get()->AddChatMessage("Not connected to any server");
			CGlobals::Get().displayServerBrowser = true;
		});
		return true;
	}
	
	if (!command.compare("/save") && CGlobals::Get().isDebug)
	{
		if (!params.size())
		{
			CChat::Get()->AddChatMessage("USAGE: /save [comment]", 0xAAAAAAFF);
			return true;
		}
		std::string comment = fullCmd.substr(command.length() + 1);
		auto playerPed = PLAYER::PLAYER_PED_ID();
		if (PED::IS_PED_IN_ANY_VEHICLE(playerPed, false))
		{
			auto pedVeh = PED::GET_VEHICLE_PED_IS_IN(playerPed, false);
			Vector3 coords = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(pedVeh, 0.0, 0.0, 0.0);
			float heading = ENTITY::GET_ENTITY_HEADING(pedVeh);
			std::ofstream saveFile(CGlobals::Get().orangePath + "\\savedcoords.txt", std::ofstream::app);
			saveFile << "{\"vehicle\": { \"coords\": { " << coords.x << ", " << coords.y << ", " << coords.z << "}, \"heading\": " << heading << " }}//" << comment << std::endl;
			saveFile.close();
		}
		else
		{
			Vector3 coords = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(playerPed, 0.0, 0.0, 0.0);
			float heading = ENTITY::GET_ENTITY_HEADING(playerPed);
			std::ofstream saveFile(CGlobals::Get().orangePath + "\\savedcoords.txt", std::ofstream::app);
			saveFile << "{\"ped\": { \"coords\": { " << coords.x << ", " << coords.y << ", " << coords.z << "}, \"heading\": " << heading << " }}//" << comment << std::endl;
			saveFile.close();
		}
		CChat::Get()->AddChatMessage("DEBUG: Your coordinates saved successfull.", 0xAAFFAAFF);
		return true;
	}
	if (!command.compare("/time") && CGlobals::Get().isDebug)
	{
		if (!params.size())
		{
			CChat::Get()->AddChatMessage("USAGE: /time [hour]", 0xAAAAAAFF);
			return true;
		}
		unsigned hour = std::stoi(params[0]);
		CScriptInvoker::Get().Push([=]() {
			TIME::SET_CLOCK_TIME(hour, 0, 0);
		});
		return true;
	}
	if (!command.compare("/weather") && CGlobals::Get().isDebug)
	{
		if (!params.size())
		{
			CChat::Get()->AddChatMessage("USAGE: /weather [weatherType]", 0xAAAAAAFF);
			return true;
		}
		std::string weather = params[0];
		CScriptInvoker::Get().Push([=]() {
			GAMEPLAY::SET_WEATHER_TYPE_NOW((char*)weather.c_str());
		});
		return true;
	}
	if (!command.compare("/vehicle") && CGlobals::Get().isDebug)
	{
		if (!params.size())
		{
			CChat::Get()->AddChatMessage("USAGE: /vehicle [modelname]", 0xAAAAAAFF);
			return true;
		}
		Hash c = Utils::Hash(params[0].c_str());
		CScriptInvoker::Get().Push([=]() {
			if (STREAMING::IS_MODEL_IN_CDIMAGE(c) && STREAMING::IS_MODEL_A_VEHICLE(c))
			{
				STREAMING::REQUEST_MODEL(c);
				while (!STREAMING::HAS_MODEL_LOADED(c))
					scriptWait(0);
				Vector3 coords = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(PLAYER::PLAYER_PED_ID(), 0.0, 5.0, 0.0);
				Vehicle veh = VEHICLE::CREATE_VEHICLE(c, coords.x, coords.y, coords.z, 0.0, 1, 1);
				VEHICLE::SET_VEHICLE_ON_GROUND_PROPERLY(veh);

				scriptWait(0);
				STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(c);
				ENTITY::SET_VEHICLE_AS_NO_LONGER_NEEDED(&veh);
			}
		});
		return true;
	}
	if (!command.compare("/snow") && CGlobals::Get().isDebug)
	{
		GameMem("SnowPatch").nop(20);
		GAMEPLAY::SET_WEATHER_TYPE_NOW_PERSIST("XMAS");
		GRAPHICS::_SET_FORCE_PED_FOOTSTEPS_TRACKS(true);
		GRAPHICS::_SET_FORCE_VEHICLE_TRAILS(true);
		return true;
	}
	if (!command.compare("/model") && CGlobals::Get().isDebug)
	{
		if (!params.size())
		{
			CChat::Get()->AddChatMessage("USAGE: /model [model id]", 0xAAAAAAFF);
			return true;
		}
		CLocalPlayer::Get()->newModel = GAMEPLAY::GET_HASH_KEY((char*)(models[std::atoi(params[0].c_str())]));
		return true;
	}	
	if (!command.compare("/debug") && CGlobals::Get().isDeveloper)
	{
		CGlobals::Get().isDebug ^= 1;
		return true;
	}
	return false;
}