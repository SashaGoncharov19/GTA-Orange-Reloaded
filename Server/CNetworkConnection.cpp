#include "stdafx.h"

CNetworkConnection * CNetworkConnection::singleInstance = nullptr;

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

static unsigned long NowMs()
{
	return (unsigned long)RakNet::GetTimeMS();
}

static float DistanceSq(const CVector3 & a, const CVector3 & b)
{
	float dx = a.fX - b.fX, dy = a.fY - b.fY, dz = a.fZ - b.fZ;
	return dx * dx + dy * dy + dz * dz;
}

// Chat and names come from clients: printable, bounded.
static std::string SanitizeText(const char * text, size_t maxLength)
{
	std::string out;
	for (const char * p = text; *p && out.size() < maxLength; ++p)
		if ((unsigned char)*p >= 0x20 || *p == '\t')
			out += *p;
	return out;
}

CNetworkConnection::CNetworkConnection()
{
	server = RakNet::RakPeerInterface::GetInstance();
}

CNetworkConnection * CNetworkConnection::Get()
{
	if (!singleInstance)
		singleInstance = new CNetworkConnection();
	return singleInstance;
}

CNetworkConnection::~CNetworkConnection()
{
	server->Shutdown(300);
	RakNet::RakPeerInterface::DestroyInstance(server);
}

void CNetworkConnection::Send(const RakNet::BitStream * bitStream, PacketPriority priority, PacketReliability reliability, char orderingChannel, const AddressOrGUID systemIdentifier, bool broadcast, int radius = 0)
{
	if (broadcast && radius != 0)
	{
		if (systemIdentifier.rakNetGuid == UNASSIGNED_RAKNET_GUID) return;

		auto player = CNetworkPlayer::GetByGUID(systemIdentifier.rakNetGuid);

		if (!player) return;

		for (auto pl : CNetworkPlayer::All())
		{
			if (pl && (player->GetPosition() - pl->GetPosition()).Length() < radius) {
				server->Send(bitStream, priority, reliability, orderingChannel, pl->GetGUID(), false);
			}
		}

	}
	else server->Send(bitStream, priority, reliability, orderingChannel, systemIdentifier, broadcast);
}

// To every player within the streaming distance of `origin`, except one.
void CNetworkConnection::RelayNear(const RakNet::BitStream * bs, const CVector3 & origin, RakNet::RakNetGUID except, PacketReliability reliability, char channel)
{
	float range = CConfig::Get()->StreamDistance;
	float range2 = range * range;
	for (CNetworkPlayer * to : CNetworkPlayer::All())
	{
		if (!to || to->GetGUID() == except)
			continue;
		if (range > 0.f && to->HasPosition() && DistanceSq(origin, to->GetPosition()) > range2)
			continue;
		server->Send(bs, MEDIUM_PRIORITY, reliability, channel, to->GetGUID(), false);
	}
}

void CNetworkConnection::SendPlayerInfo(CNetworkPlayer * about, const AddressOrGUID & to, bool broadcast)
{
	RakNet::BitStream bs;
	bs.Write((unsigned char)ID_PLAYER_INFO);
	bs.Write(about->GetGUID());
	bs.Write((unsigned int)about->GetID());
	bs.Write(RakNet::RakString(about->GetName().c_str()));
	bs.Write((Hash)about->GetModel());
	bs.Write(about->GetColor());
	server->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, ORANGE_CHANNEL_RELIABLE, to, broadcast);
}

// The 2017 client knows nothing of snapshots: it learns about the others
// from the relayed ID_SEND_PLAYER_DATA (GUID, name, state) and nothing else.
// Kept for players that announced protocol 1, within stream distance.
void CNetworkConnection::RelayLegacyState(CNetworkPlayer * from, const OnFootSyncData & data)
{
	if (!CNetworkPlayer::LegacyCount())
		return;
	const float range = CConfig::Get()->StreamDistance;
	RakNet::BitStream bs;
	bs.Write((unsigned char)ID_SEND_PLAYER_DATA);
	bs.Write(from->GetGUID());
	bs.Write(RakNet::RakString(from->GetName().c_str()));
	bs.Write(data);
	for (CNetworkPlayer * to : CNetworkPlayer::All())
	{
		if (!to || to == from || !to->IsLegacy())
			continue;
		if (range > 0.f && to->HasPosition() && DistanceSq(to->GetPosition(), data.vecPos) > range * range)
			continue;
		server->Send(&bs, MEDIUM_PRIORITY, RELIABLE_ORDERED, ORANGE_CHANNEL_RELIABLE, to->GetGUID(), false);
	}
}

void CNetworkConnection::Kick(CNetworkPlayer * player, const char * reason)
{
	if (!player)
		return;
	log << "Kicking player " << player->GetID() << " '" << player->GetName() << "': " << reason << std::endl;
	RakNet::BitStream bsOut;
	bsOut.Write(RakNet::RakString((std::string("Kicked: ") + reason).c_str()));
	color_t messageColor = { 255, 90, 90, 255 };
	bsOut.Write(messageColor);
	CRPCPlugin::Get()->Signal("SendClientMessage", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, player->GetGUID(), false, false);
	// The disconnection notification arrives through Tick and removes the
	// player there, like any other leaver.
	server->CloseConnection(player->GetGUID(), true, 0, LOW_PRIORITY);
}

bool CNetworkConnection::Start(unsigned short maxPlayers, unsigned short port)
{
	if (maxPlayers && port)
	{
		socketDescriptors[0].port = port;
		socketDescriptors[0].socketFamily = AF_INET; // Test out IPV4
		socketDescriptors[1].port = port;
		socketDescriptors[1].socketFamily = AF_INET6; // Test out IPV6
		// First try dual stack (IPv4 + IPv6). RakNet prints "Unknown bind__() error"
		// itself when the IPv6 socket cannot be bound (no IPv6 on the host, e.g.
		// inside containers); in that case we fall back to IPv4 only.
		bool result = server->Startup(maxPlayers, socketDescriptors, 2) == RakNet::RAKNET_STARTED;
		if (!result)
		{
			log << "IPv6 socket unavailable, falling back to IPv4 only" << std::endl;
			result = server->Startup(maxPlayers, socketDescriptors, 1) == RakNet::RAKNET_STARTED;
			if (!result)
			{
				log_error << "Server not started: could not bind UDP port " << port << std::endl;
				exit(EXIT_FAILURE);
			}
			else
				log << "Server started on UDP port " << port << " (IPv4)" << std::endl;
		} else log << "Server started on UDP port " << port << " (IPv4 + IPv6)" << std::endl;
		server->SetMaximumIncomingConnections(maxPlayers);
		server->SetTimeoutTime(15000, RakNet::UNASSIGNED_SYSTEM_ADDRESS);
		const CConfig * cfg = CConfig::Get();
		log << "Sync: " << cfg->SyncRate << " snapshots/s, stream distance "
			<< (cfg->StreamDistance > 0.f ? std::to_string((int)cfg->StreamDistance) + " m" : std::string("unlimited"))
			<< ", at most " << cfg->MaxStreamedPlayers << " players per snapshot, up to " << cfg->MaxClientSyncRate << " packets/s per client" << std::endl;
		ulLastStatsMs = NowMs();
		return true;
	}
	return false;
}

// The player behind a packet, or nullptr with one log line per stranger: a
// client that sends anything but ID_CONNECT_TO_SERVER first is not a player.
static CNetworkPlayer * PlayerOf(RakNet::Packet * packet, const char * what)
{
	CNetworkPlayer * player = CNetworkPlayer::GetByGUID(packet->guid);
	if (!player)
	{
		static std::set<uint64_t> said;
		if (said.size() < 1024 && said.insert(packet->guid.g).second)
			log << what << " from " << packet->systemAddress.ToString(true) << " before it joined, ignored" << std::endl;
	}
	return player;
}

void CNetworkConnection::Tick()
{
	const CConfig * cfg = CConfig::Get();
	auto receiveStarted = std::chrono::steady_clock::now();
	ulTicks++;
	for (packet = server->Receive(); packet; server->DeallocatePacket(packet), packet = server->Receive())
	{
		ulPacketsIn++;
		unsigned char packetIdentifier = packet->data[0];
		RakNet::BitStream bsIn(packet->data, packet->length, false);
		bsIn.IgnoreBytes(sizeof(unsigned char));
		RakNet::BitStream bsOut;

		switch (packetIdentifier)
		{
			case ID_DISCONNECTION_NOTIFICATION:
			case ID_CONNECTION_LOST:
			{
				int reason = packetIdentifier == ID_DISCONNECTION_NOTIFICATION ? 1 : 2;
				CNetworkPlayer *player = CNetworkPlayer::GetByGUID(packet->guid);
				if (!player)
				{
					log << "Connection " << packet->systemAddress.ToString(true) << (reason == 1 ? " closed" : " lost") << " before joining" << std::endl;
					break;
				}
				UINT playerID = player->GetID();
				log << "Player " << playerID << " '" << player->GetName() << "' " << (reason == 1 ? "disconnected" : "lost connection")
					<< " (" << packet->systemAddress.ToString(true) << ")" << std::endl;

				Plugin::PlayerDisconnect(playerID, reason);
				Plugin::Trigger("PlayerDisconnect", (unsigned long)playerID, reason);

				CNetworkPlayer::Remove(playerID);

				bsOut.Write((unsigned char)ID_PLAYER_LEFT);
				bsOut.Write(packet->guid);
				server->Send(&bsOut, HIGH_PRIORITY, RELIABLE_ORDERED, ORANGE_CHANNEL_RELIABLE, packet->guid, true);
				break;
			}
			case ID_NEW_INCOMING_CONNECTION:
			{
				log << "Incoming connection from " << packet->systemAddress.ToString(true) << std::endl;
				bsOut.Write(UsedModels.size());
				for (Hash m : UsedModels) bsOut.Write(m);
				CRPCPlugin::Get()->Signal("PreloadModels", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, packet->systemAddress, false, false);

				CNetworkBlip::SendGlobal(packet);
				CNetworkMarker::SendGlobal(packet);
				CNetworkVehicle::SendGlobal(packet);
				CNetwork3DText::SendGlobal(packet);
				CNetworkObject::SendGlobal(packet);
				CClientScripting::SendGlobal(packet);

				break;
			}
			case ID_CONNECT_TO_SERVER:
			{
				if (CNetworkPlayer::GetByGUID(packet->guid))
				{
					log << "Second ID_CONNECT_TO_SERVER from " << packet->systemAddress.ToString(true) << ", ignored" << std::endl;
					break;
				}
				RakNet::RakString playerName;
				bsIn.Read(playerName);
				// The 2017 client and orange_handshake send the name only;
				// current clients add their version and protocol number.
				RakNet::RakString clientVersion("unknown");
				unsigned int protocol = 1;
				if (bsIn.GetNumberOfUnreadBits() >= 16)
				{
					bsIn.Read(clientVersion);
					if (bsIn.GetNumberOfUnreadBits() >= 32)
						bsIn.Read(protocol);
				}
				if (CNetworkPlayer::Count() >= cfg->MaxPlayers)
				{
					log << "Server full (" << cfg->MaxPlayers << "), refusing " << packet->systemAddress.ToString(true) << std::endl;
					server->CloseConnection(packet->guid, true, 0, LOW_PRIORITY);
					break;
				}
				CNetworkPlayer *player = CNetworkPlayer::Create(packet->guid, packet->systemAddress);
				std::string name = SanitizeText(playerName.C_String(), 31);
				if (name.empty())
					name = "Player" + std::to_string(player->GetID());
				player->SetName(name);
				player->SetClientVersion(SanitizeText(clientVersion.C_String(), 63));
				player->SetProtocol(protocol);
				log << "Player " << player->GetID() << " '" << name << "' joined from " << packet->systemAddress.ToString(true)
					<< " (client " << player->GetClientVersion() << ", protocol " << protocol << ")" << std::endl;

				Plugin::PlayerConnect(player->GetID());
				Plugin::Trigger("PlayerConnect", (unsigned long)player->GetID());

				bsOut.Write((unsigned char)ID_CONNECT_TO_SERVER);
				server->Send(&bsOut, HIGH_PRIORITY, RELIABLE_ORDERED, ORANGE_CHANNEL_RELIABLE, packet->systemAddress, false);

				// Who is who: the newcomer learns every player, everyone learns
				// the newcomer. Protocol 1 clients get names inside the relayed
				// state instead (RelayLegacyState).
				for (CNetworkPlayer * other : CNetworkPlayer::All())
				{
					if (!other || other == player)
						continue;
					if (!player->IsLegacy())
						SendPlayerInfo(other, packet->guid, false);
					if (!other->IsLegacy())
						SendPlayerInfo(player, other->GetGUID(), false);
				}
				break;
			}
			case ID_CHAT_MESSAGE:
			{
				CNetworkPlayer * player = PlayerOf(packet, "Chat");
				if (!player) break;
				RakNet::RakString playerText;
				bsIn.Read(playerText);
				std::string text = SanitizeText(playerText.C_String(), 256);
				if (text.empty()) break;

				if (Plugin::PlayerText(player->GetID(), text.c_str()))
				{
					std::stringstream ss;
					ss << player->GetName() << " " << u8"" << " {FFFFFF}" << text;
					RakNet::RakString toSend(ss.str().c_str());
					bsOut.Write(toSend);
					color_t messageColor = { 0x7C, 0xB9, 0xE8, 0xFF };
					bsOut.Write(messageColor);
					CRPCPlugin::Get()->Signal("SendClientMessage", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true, false);
				}
				break;
			}
			case ID_COMMAND_MESSAGE:
			{
				CNetworkPlayer * player = PlayerOf(packet, "Command");
				if (!player) break;
				RakNet::RakString playerText;
				bsIn.Read(playerText);
				std::string text = SanitizeText(playerText.C_String(), 256);

				if (Plugin::PlayerCommand(player->GetID(), text.c_str()))
				{
					RakNet::RakString toSend("Unknown command");
					bsOut.Write(toSend);
					color_t messageColor = { 255, 200, 200, 255 };
					bsOut.Write(messageColor);
					CRPCPlugin::Get()->Signal("SendClientMessage", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, packet->guid, false, false);
				}
				break;
			}
			case ID_SEND_PLAYER_DATA:
			{
				CNetworkPlayer *player = PlayerOf(packet, "Player data");
				if (!player) break;
				if (bsIn.GetNumberOfUnreadBits() < sizeof(OnFootSyncData) * 8)
					break;   // truncated
				OnFootSyncData data;
				bsIn.Read(data);
				if (!player->AcceptSync(NowMs(), cfg->MaxClientSyncRate))
					break;   // over the per-client cap: the state we have is fresh enough
				player->SetOnFootData(data);
				Plugin::PlayerUpdate(player->GetID());
				// Relayed to current clients by SendSnapshots, not here.
				RelayLegacyState(player, data);
				break;
			}
			case ID_SEND_VEHICLE_DATA:
			{
				CNetworkPlayer *player = PlayerOf(packet, "Vehicle data");
				if (!player) break;
				if (bsIn.GetNumberOfUnreadBits() < sizeof(VehicleData) * 8)
					break;
				VehicleData data;
				bsIn.Read(data);

				if (data.GUID == UNASSIGNED_RAKNET_GUID) break;
				CNetworkVehicle *veh = CNetworkVehicle::GetByGUID(data.GUID);
				if (!veh) break;

				unsigned long now = NowMs();
				// The vehicle's state comes from its driver; another client's
				// packets about it are ignored while the driver keeps sending.
				if (!veh->AcceptsStateFrom(packet->guid, now))
					break;
				if (data.hasDriver) data.driver = packet->guid;
				veh->ulLastUpdateMs = now;
				veh->SetVehicleData(data);
				veh->GetVehicleData(data);

				bsOut.Write((unsigned char)ID_SEND_VEHICLE_DATA);
				bsOut.Write(data);
				RelayNear(&bsOut, data.vecPos, packet->guid, UNRELIABLE_SEQUENCED, ORANGE_CHANNEL_STATE);
				break;
			}
			case ID_SEND_TASKS:
			{
				CNetworkPlayer *player = PlayerOf(packet, "Task data");
				if (!player) break;
				int tasks = 0;
				bsIn.Read(tasks);
				if (tasks < 0 || tasks > 64) break;
				bsOut.Write((unsigned char)ID_SEND_TASKS);
				bsOut.Write(packet->guid);
				bsOut.Write(tasks);
				bool ok = true;
				for (int i = 0; i < tasks && ok; ++i)
				{
					unsigned short taskID;
					unsigned int size;
					if (!bsIn.Read(taskID) || !bsIn.Read(size) || size > 65536 || bsIn.GetNumberOfUnreadBits() < size)
					{
						ok = false;
						break;
					}
					bsOut.Write(taskID);
					bsOut.Write(size);

					int bytesSize = (size % 8) ? (size / 8 + 1) : (size / 8);
					std::vector<unsigned char> taskInfo(bytesSize ? bytesSize : 1);
					bsIn.ReadBits(taskInfo.data(), size);
					bsOut.WriteBits(taskInfo.data(), size);
				}
				if (ok)
					RelayNear(&bsOut, player->GetPosition(), packet->guid, RELIABLE_ORDERED, ORANGE_CHANNEL_RELIABLE);
				break;
			}
			case ID_CONNECTED_PING:
			case ID_UNCONNECTED_PING:
			case ID_UNCONNECTED_PONG:
			case ID_CONNECTED_PONG:
				break;
			default:
			{
				if (unknownIds.insert(packetIdentifier).second)
					log << "Unknown packet id " << (int)packetIdentifier << " from " << packet->systemAddress.ToString(true) << " (" << packet->length << " bytes), ignored from now on" << std::endl;
				break;
			}
		}
	}

	dReceiveSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - receiveStarted).count();

	unsigned long now = NowMs();
	SendSnapshots(now);
	if (now - ulLastStatsMs >= 30000)
		LogStats(now);
}

// Every 30 s: what the server is doing, summed over all connections.
void CNetworkConnection::LogStats(unsigned long nowMs)
{
	double seconds = ulLastStatsMs ? (nowMs - ulLastStatsMs) / 1000.0 : 30.0;
	if (seconds <= 0.0) seconds = 30.0;

	DataStructures::List<SystemAddress> addresses;
	DataStructures::List<RakNetGUID> guids;
	DataStructures::List<RakNetStatistics> stats;
	server->GetStatisticsList(addresses, guids, stats);
	unsigned long long bytesIn = 0, bytesOut = 0;
	double loss = 0.0;
	for (unsigned int i = 0; i < stats.Size(); i++)
	{
		bytesIn += stats[i].valueOverLastSecond[ACTUAL_BYTES_RECEIVED];
		bytesOut += stats[i].valueOverLastSecond[ACTUAL_BYTES_SENT];
		loss += stats[i].packetlossLastSecond;
	}
	if (stats.Size()) loss /= stats.Size();

	log << "Stats: " << CNetworkPlayer::Count() << " player(s); loop " << (unsigned long)(ulTicks / seconds) << "/s, "
		<< (unsigned long)(ulPacketsIn / seconds) << " packet(s)/s in ("
		<< std::fixed << std::setprecision(1) << (dReceiveSeconds / seconds * 100.0) << "% of a core); "
		<< (unsigned long)(ulSnapshotsSent / seconds) << " snapshot datagram(s)/s with "
		<< (ulSnapshotsSent ? ulSnapshotEntries / ulSnapshotsSent : 0) << " player(s) each ("
		<< (dSnapshotSeconds / seconds * 100.0) << "% of a core); "
		<< bytesIn / 1024 << " KB/s in, " << bytesOut / 1024 << " KB/s out"
		<< (stats.Size() ? " (" + std::to_string(bytesOut / 1024 / stats.Size()) + " KB/s per client)" : "")
		<< ", packet loss " << std::setprecision(2) << loss * 100.0 << "%" << std::endl;

	ulLastStatsMs = nowMs;
	ulTicks = 0;
	ulPacketsIn = 0;
	ulSnapshotsSent = 0;
	ulSnapshotEntries = 0;
	dReceiveSeconds = 0.0;
	dSnapshotSeconds = 0.0;
}

// Player state goes out as ID_PLAYER_SNAPSHOT datagrams, one batch per
// player per sync period: the states of the players around it. Distance
// decides how often a player appears in someone else's batch: within a fifth
// of the stream distance every period, within three fifths every second one,
// beyond that every fourth one, and never beyond the stream distance. When
// more players are near than max_streamed_players allows, the nearest win.
//
// A batch is cut into datagrams that fit the receiver's MTU: RakNet turns any
// unreliable message larger than one datagram into a reliable one, which is
// exactly what state must not be. Each datagram carries the server time it
// was built at, so the receiver can drop an entry older than the state it
// already has, whatever order the datagrams arrive in.
//
// Neighbours are found through a grid of stream_distance-sized cells, so the
// cost grows with the number of players near each other, not with the square
// of everyone online.
static inline unsigned long long CellKey(float x, float y, float cell)
{
	long long cx = (long long)std::floor(x / cell);
	long long cy = (long long)std::floor(y / cell);
	return ((unsigned long long)(unsigned int)(int)cx << 32) | (unsigned int)(int)cy;
}

void CNetworkConnection::SendSnapshots(unsigned long nowMs)
{
	const CConfig * cfg = CConfig::Get();
	unsigned int rate = cfg->SyncRate ? cfg->SyncRate : 20;
	unsigned long period = 1000 / rate;
	if (ulLastSnapshotMs && nowMs - ulLastSnapshotMs < period)
		return;
	ulLastSnapshotMs = nowMs;
	ulSnapshotTick++;

	auto started = std::chrono::steady_clock::now();
	const float range = cfg->StreamDistance;
	const float near2 = (range * 0.2f) * (range * 0.2f);
	const float mid2 = (range * 0.6f) * (range * 0.6f);
	const float far2 = range * range;
	const size_t cap = cfg->MaxStreamedPlayers ? cfg->MaxStreamedPlayers : ORANGE_MAX_SNAPSHOT_ENTRIES;
	const auto & all = CNetworkPlayer::All();

	// 1. Who can be seen: players with a position that is not stale. A client
	// that stopped sending is stale after 5 s; it drops out of the snapshots
	// and the receivers time it out themselves.
	snapshotSources.clear();
	for (CNetworkPlayer * from : all)
	{
		if (!from || !from->HasPosition() || nowMs - from->LastSyncMs() > 5000)
			continue;
		const CVector3 & p = from->GetPosition();
		snapshotSources.push_back(SnapshotSource{ p.fX, p.fY, p.fZ, from });
	}
	if (snapshotSources.empty())
		return;

	// 2. Sort them into cells of one stream distance; a receiver then only
	// looks at its own cell and the eight around it.
	const bool useGrid = range > 0.f;
	if (useGrid)
	{
		snapshotOrder.resize(snapshotSources.size());
		for (unsigned int i = 0; i < snapshotOrder.size(); i++)
			snapshotOrder[i] = i;
		std::sort(snapshotOrder.begin(), snapshotOrder.end(), [&](unsigned int a, unsigned int b)
		{
			return CellKey(snapshotSources[a].x, snapshotSources[a].y, range) < CellKey(snapshotSources[b].x, snapshotSources[b].y, range);
		});
		snapshotCells.clear();
		for (unsigned int i = 0; i < snapshotOrder.size();)
		{
			unsigned long long key = CellKey(snapshotSources[snapshotOrder[i]].x, snapshotSources[snapshotOrder[i]].y, range);
			unsigned int j = i + 1;
			while (j < snapshotOrder.size() && CellKey(snapshotSources[snapshotOrder[j]].x, snapshotSources[snapshotOrder[j]].y, range) == key)
				j++;
			snapshotCells[key] = std::make_pair(i, j);
			i = j;
		}
	}

	// 3. One batch per receiver.
	RakNet::BitStream bs;
	auto consider = [&](const SnapshotSource & src, CNetworkPlayer * to, const CVector3 & at)
	{
		if (src.player == to)
			return;
		float d2 = 0.f;
		if (useGrid)
		{
			float dx = src.x - at.fX, dy = src.y - at.fY, dz = src.z - at.fZ;
			d2 = dx * dx + dy * dy + dz * dz;
			if (d2 > far2)
				return;
			if (d2 > mid2) { if (ulSnapshotTick % 4) return; }
			else if (d2 > near2) { if (ulSnapshotTick % 2) return; }
		}
		snapshotNear.push_back(std::make_pair(d2, src.player));
	};

	for (CNetworkPlayer * to : all)
	{
		if (!to)
			continue;
		// nobody to stream around a player who has not reported a position
		// yet; a 2017 client is served by RelayLegacyState
		if ((useGrid && !to->HasPosition()) || to->IsLegacy())
			continue;
		const CVector3 & at = to->GetPosition();
		snapshotNear.clear();
		if (useGrid)
		{
			long long cx = (long long)std::floor(at.fX / range);
			long long cy = (long long)std::floor(at.fY / range);
			for (long long dx = -1; dx <= 1; dx++)
				for (long long dy = -1; dy <= 1; dy++)
				{
					unsigned long long key = ((unsigned long long)(unsigned int)(int)(cx + dx) << 32) | (unsigned int)(int)(cy + dy);
					auto cell = snapshotCells.find(key);
					if (cell == snapshotCells.end())
						continue;
					for (unsigned int i = cell->second.first; i < cell->second.second; i++)
						consider(snapshotSources[snapshotOrder[i]], to, at);
				}
		}
		else
		{
			for (const SnapshotSource & src : snapshotSources)
				consider(src, to, at);
		}
		if (snapshotNear.empty())
			continue;
		if (snapshotNear.size() > cap)
		{
			std::partial_sort(snapshotNear.begin(), snapshotNear.begin() + cap, snapshotNear.end(),
				[](const std::pair<float, CNetworkPlayer*> & a, const std::pair<float, CNetworkPlayer*> & b) { return a.first < b.first; });
			snapshotNear.resize(cap);
		}

		// Datagram budget: the path MTU less IP/UDP headers (28) and RakNet's
		// own headers (at most 32), less our 6-byte header.
		int mtu = server->GetMTUSize(to->GetAddress());
		if (mtu <= 0) mtu = MINIMUM_MTU_SIZE;
		const size_t entryBytes = sizeof(RakNetGUID) + sizeof(OnFootSyncData);
		size_t perDatagram = ((size_t)mtu - 28 - 32 - 6) / entryBytes;
		if (perDatagram < 1) perDatagram = 1;

		for (size_t offset = 0; offset < snapshotNear.size(); offset += perDatagram)
		{
			size_t count = snapshotNear.size() - offset;
			if (count > perDatagram) count = perDatagram;   // no std::min: windows.h defines min
			bs.Reset();
			bs.Write((unsigned char)ID_PLAYER_SNAPSHOT);
			bs.Write((unsigned int)nowMs);
			bs.Write((unsigned char)count);
			for (size_t i = offset; i < offset + count; i++)
			{
				bs.Write(snapshotNear[i].second->GetGUID());
				bs.Write(snapshotNear[i].second->LastSync());
			}
			server->Send(&bs, MEDIUM_PRIORITY, UNRELIABLE, ORANGE_CHANNEL_STATE, to->GetGUID(), false);
			ulSnapshotsSent++;
			ulSnapshotEntries += count;
		}
	}
	dSnapshotSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
}
