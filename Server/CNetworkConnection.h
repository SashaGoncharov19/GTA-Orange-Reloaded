#pragma once

class CNetworkPlayer;
class CVector3;
struct OnFootSyncData;

// The RakNet side of the server: accepts connections, turns packets into
// player state and events, and sends every player a snapshot of the players
// around it at a fixed rate (SendSnapshots), instead of forwarding each
// received packet to everybody.
class CNetworkConnection
{
	static CNetworkConnection * singleInstance;
	CNetworkConnection();

	RPC4 rpc;
	RakNet::Packet* packet;
	RakNet::SocketDescriptor socketDescriptors[2];
	RakNet::ConnectionAttemptResult connection;
	bool bConnected = false;

	unsigned long ulLastSnapshotMs = 0;
	unsigned long ulSnapshotTick = 0;
	unsigned long ulLastStatsMs = 0;
	unsigned long ulTicks = 0;               // network loop iterations since the last statistics line
	unsigned long ulPacketsIn = 0;           // packets handled in them
	unsigned long ulSnapshotsSent = 0;       // datagrams of player state sent
	unsigned long ulSnapshotEntries = 0;     // player states inside them
	double dReceiveSeconds = 0.0;            // CPU time spent handling packets
	double dSnapshotSeconds = 0.0;           // CPU time spent building snapshots
	std::set<unsigned char> unknownIds;

	// Scratch space for SendSnapshots, kept between ticks to avoid reallocating.
	struct SnapshotSource
	{
		float x, y, z;
		CNetworkPlayer * player;
	};
	std::vector<SnapshotSource> snapshotSources;
	std::vector<unsigned int> snapshotOrder;                                      // indices into snapshotSources sorted by cell
	std::unordered_map<unsigned long long, std::pair<unsigned int, unsigned int>> snapshotCells; // cell -> [begin, end) in snapshotOrder
	std::vector<std::pair<float, CNetworkPlayer*>> snapshotNear;

	void LogStats(unsigned long nowMs);

	void SendPlayerInfo(CNetworkPlayer * about, const AddressOrGUID & to, bool broadcast);
	void RelayLegacyState(CNetworkPlayer * from, const OnFootSyncData & data);
	void RelayNear(const RakNet::BitStream * bs, const CVector3 & origin, RakNet::RakNetGUID except, PacketReliability reliability, char channel);
public:
	RakNet::RakPeerInterface *server;
	std::vector<Hash> UsedModels;

	static CNetworkConnection * Get();

	void Send(const RakNet::BitStream * bitStream, PacketPriority priority, PacketReliability reliability, char orderingChannel, const AddressOrGUID systemIdentifier, bool broadcast, int radius);

	bool Start(unsigned short maxPlayers, unsigned short port);
	void Tick();
	// Builds and sends ID_PLAYER_SNAPSHOT to every player when a sync period
	// has passed; called from the network loop.
	void SendSnapshots(unsigned long nowMs);
	// Disconnects a player with a reason shown in their chat.
	void Kick(CNetworkPlayer * player, const char * reason);

	~CNetworkConnection();
};
