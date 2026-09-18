#include "stdafx.h"

#ifndef _WIN32
#include <unistd.h>
#endif

CConfig* CConfig::singleInstance = nullptr;


CConfig::CConfig()
{
	std::ifstream fin("config.yml");
	YAML::Parser parser(fin);

	YAML::Node doc;
	if (!parser.GetNextDocument(doc)) {
		log << "Can`t load config.yml" << std::endl;
		Hostname.append("Unnamed server");
		Port = 7788;
		HTTPPort = 7789;
		MaxPlayers = 128;
	}
	else {
		if(!doc.FindValue("name")) Hostname.append("Unnamed server");
		else doc["name"] >> Hostname;
		if (!doc.FindValue("port")) Port = 7788;
		else doc["port"] >> Port;
		if (!doc.FindValue("httpport"))
			if (!doc.FindValue("port")) HTTPPort = 7789;
			else HTTPPort = Port + 1;
		else doc["httpport"] >> HTTPPort;
		if (!doc.FindValue("players")) MaxPlayers = 128;
		else doc["players"] >> MaxPlayers;
		if (doc.FindValue("stream_distance")) doc["stream_distance"] >> StreamDistance;
		if (doc.FindValue("sync_rate")) doc["sync_rate"] >> SyncRate;
		if (doc.FindValue("max_streamed_players")) doc["max_streamed_players"] >> MaxStreamedPlayers;
		if (doc.FindValue("max_client_sync_rate")) doc["max_client_sync_rate"] >> MaxClientSyncRate;

		const YAML::Node& resources = doc["resources"];

		for (unsigned i = 0; i < resources.size(); i++) {
			std::string res;;
			resources[i] >> res;
			Resources.push_back(res);
		}
	}
	if (MaxPlayers > 4096) {
		log << "players: at most 4096 connections, set to 4096" << std::endl;
		MaxPlayers = 4096;
	}
	if (SyncRate < 1) SyncRate = 1;
	if (SyncRate > 60) SyncRate = 60;
	if (MaxStreamedPlayers > ORANGE_MAX_SNAPSHOT_ENTRIES) MaxStreamedPlayers = ORANGE_MAX_SNAPSHOT_ENTRIES;
	if (StreamDistance < 0.f) StreamDistance = 0.f;

	char buffer[MAX_PATH];
#ifdef _WIN32
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	Path = std::string(buffer).substr(0, pos);
#else
	getcwd(buffer, MAX_PATH);
	Path = std::string(buffer);
#endif
}

CConfig* CConfig::Get()
{
	if (!singleInstance)
		singleInstance = new CConfig();
	return singleInstance;
}


CConfig::~CConfig()
{
}
