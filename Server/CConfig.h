#pragma once
class CConfig
{
	CConfig();

	static CConfig* singleInstance;
public:
	static CConfig* Get();
	std::string Path;
	std::string Hostname;
	std::vector<std::string> Resources;
	unsigned short Port;
	unsigned short HTTPPort;
	unsigned short MaxPlayers;
	// Synchronisation (config.yml keys in the comments):
	float StreamDistance = 500.f;        // stream_distance: metres; a player only receives the state of players within it (0 = everyone)
	unsigned int SyncRate = 20;          // sync_rate: snapshots per second the server sends to every player
	unsigned int MaxStreamedPlayers = 32;// max_streamed_players: at most this many (the nearest) per snapshot
	unsigned int MaxClientSyncRate = 40; // max_client_sync_rate: on-foot packets per second accepted from one client
	~CConfig();
};

