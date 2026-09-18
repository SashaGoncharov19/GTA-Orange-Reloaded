// orange_bot: N simulated players for load and behaviour tests of
// orange_server, no game needed. Every bot is its own RakNet peer: it joins
// like the game client (ID_CONNECT_TO_SERVER with a name, client version and
// protocol number), then sends on-foot state at the client's rate while
// walking a small circle around its spot, and counts what the server sends
// back (ID_PLAYER_SNAPSHOT entries, ID_PLAYER_INFO records).
//
//   orange_bot [--host 127.0.0.1] [--port 7788] [--bots 20] [--duration 10]
//              [--spread 200] [--rate 20] [--threads N] [--center x,y,z]
//              [--protocol 2] [--quiet]
//
// --spread is the radius (metres) the bots are placed in around --center:
// with a spread larger than the server's stream_distance the bots only see
// their neighbours, which is what the streaming code is for. --protocol 1
// joins the way the 2017 client does (name only) and expects the relayed
// ID_SEND_PLAYER_DATA packets instead of snapshots.
//
// Exit code 0 when every bot was accepted and (with more than one bot) every
// bot received at least one snapshot entry; 1 otherwise.
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "RakNetTypes.h"
#include "RakString.h"
#include "RakSleep.h"
#include "GetTime.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <set>
#include <mutex>
#include <chrono>

// The shared headers expect the server's typedefs and RakNet namespace.
#ifndef _WIN32
typedef unsigned char BYTE;
typedef unsigned int UINT;
#endif
using namespace RakNet;
#include "CMath.h"
#include "types.h"
#include "CVector3.h"
#include "NetworkTypes.h"

#ifndef ORANGE_VERSION
#define ORANGE_VERSION "dev"
#endif

struct Options
{
	std::string host = "127.0.0.1";
	unsigned short port = 7788;
	int bots = 20;
	double duration = 10.0;
	float spread = 200.f;
	int rate = ORANGE_CLIENT_SYNC_RATE_ON_FOOT;
	unsigned int protocol = ORANGE_PROTOCOL_VERSION;   // 1 behaves like the 2017 client
	int threads = 0;
	CVector3 center = CVector3(21.24f, -711.04f, 45.97f);
	bool quiet = false;
};

struct Bot
{
	int index = 0;
	RakNet::RakPeerInterface * peer = nullptr;
	RakNet::SystemAddress server;
	bool accepted = false;
	bool established = false;
	bool failed = false;
	CVector3 home;
	float phase = 0.f;
	unsigned long nextSendMs = 0;
	unsigned long snapshots = 0;
	unsigned long entries = 0;
	unsigned long infos = 0;
	unsigned long lastSnapshotMs = 0;
	std::set<uint64_t> seen;
	unsigned char maxEntries = 0;
};

static std::atomic<bool> g_running(true);

static void FillState(OnFootSyncData & s, const CVector3 & pos, float heading)
{
	memset(&s, 0, sizeof(s));
	s.hModel = 0x705E61F2;   // mp_m_freemode_01
	s.usHealth = 200;
	s.usArmour = 0;
	s.fMoveSpeed = 1.5f;
	s.fHeading = heading;
	s.vecPos = pos;
	s.vecRot = CVector3(0.f, 0.f, heading);
	s.vecMoveSpeed = CVector3(cosf(heading) * 1.5f, sinf(heading) * 1.5f, 0.f);
	s.rnVehicle = RakNet::UNASSIGNED_RAKNET_GUID;
	s.cSeat = -2;
}

static void BotStart(Bot & bot, const Options & opt)
{
	bot.peer = RakNet::RakPeerInterface::GetInstance();
	RakNet::SocketDescriptor sd(0, 0);
	sd.socketFamily = AF_INET;
	if (bot.peer->Startup(1, &sd, 1) != RakNet::RAKNET_STARTED)
	{
		bot.failed = true;
		return;
	}
	if (bot.peer->Connect(opt.host.c_str(), opt.port, 0, 0) != RakNet::CONNECTION_ATTEMPT_STARTED)
		bot.failed = true;
}

static void BotPump(Bot & bot, const Options & opt, unsigned long nowMs)
{
	if (bot.failed || !bot.peer)
		return;
	for (RakNet::Packet * p = bot.peer->Receive(); p; bot.peer->DeallocatePacket(p), p = bot.peer->Receive())
	{
		unsigned char id = p->data[0];
		RakNet::BitStream in(p->data, p->length, false);
		in.IgnoreBytes(1);
		switch (id)
		{
		case ID_CONNECTION_REQUEST_ACCEPTED:
		{
			bot.accepted = true;
			bot.server = p->systemAddress;
			RakNet::BitStream out;
			out.Write((unsigned char)ID_CONNECT_TO_SERVER);
			out.Write(RakNet::RakString(("bot" + std::to_string(bot.index)).c_str()));
			if (opt.protocol >= 2)
			{
				out.Write(RakNet::RakString("orange_bot " ORANGE_VERSION));
				out.Write((unsigned int)opt.protocol);
			}
			bot.peer->Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, ORANGE_CHANNEL_RELIABLE, p->systemAddress, false);
			break;
		}
		case ID_CONNECT_TO_SERVER:
			bot.established = true;
			bot.nextSendMs = nowMs;
			break;
		case ID_PLAYER_SNAPSHOT:
		{
			unsigned int serverTime = 0;
			unsigned char count = 0;
			in.Read(serverTime);
			in.Read(count);
			bot.snapshots++;
			bot.lastSnapshotMs = nowMs;
			if (count > bot.maxEntries) bot.maxEntries = count;
			for (unsigned char i = 0; i < count; i++)
			{
				RakNet::RakNetGUID guid;
				OnFootSyncData state;
				if (!in.Read(guid) || !in.Read(state))
					break;
				bot.entries++;
				if (bot.seen.size() < 4096)
					bot.seen.insert(guid.g);
			}
			break;
		}
		case ID_PLAYER_INFO:
			bot.infos++;
			break;
		case ID_SEND_PLAYER_DATA:   // the 2017 relay: GUID, name, state
		{
			RakNet::RakNetGUID guid;
			RakNet::RakString name;
			OnFootSyncData state;
			if (in.Read(guid) && in.Read(name) && in.Read(state))
			{
				bot.snapshots++;
				bot.entries++;
				bot.lastSnapshotMs = nowMs;
				if (bot.maxEntries < 1) bot.maxEntries = 1;
				if (bot.seen.size() < 4096)
					bot.seen.insert(guid.g);
			}
			break;
		}
		case ID_CONNECTION_ATTEMPT_FAILED:
		case ID_NO_FREE_INCOMING_CONNECTIONS:
		case ID_DISCONNECTION_NOTIFICATION:
		case ID_CONNECTION_LOST:
		case ID_CONNECTION_BANNED:
			bot.failed = true;
			bot.established = false;
			break;
		default:
			break;
		}
	}
	if (bot.established && opt.rate > 0 && (long)(nowMs - bot.nextSendMs) >= 0)
	{
		bot.nextSendMs = nowMs + 1000 / opt.rate;
		// a slow circle of radius 4 m around home
		bot.phase += 0.05f;
		CVector3 pos(bot.home.fX + cosf(bot.phase) * 4.f, bot.home.fY + sinf(bot.phase) * 4.f, bot.home.fZ);
		OnFootSyncData state;
		FillState(state, pos, bot.phase);
		RakNet::BitStream out;
		out.Write((unsigned char)ID_SEND_PLAYER_DATA);
		out.Write(state);
		bot.peer->Send(&out, MEDIUM_PRIORITY, UNRELIABLE_SEQUENCED, ORANGE_CHANNEL_STATE, bot.server, false);
	}
}

static bool ParseVec(const char * text, CVector3 & out)
{
	float x, y, z;
	if (sscanf(text, "%f,%f,%f", &x, &y, &z) != 3)
		return false;
	out = CVector3(x, y, z);
	return true;
}

int main(int argc, char ** argv)
{
	Options opt;
	for (int i = 1; i < argc; i++)
	{
		std::string a = argv[i];
		const char * v = i + 1 < argc ? argv[i + 1] : "";
		if (a == "--host") { opt.host = v; i++; }
		else if (a == "--port") { opt.port = (unsigned short)atoi(v); i++; }
		else if (a == "--bots") { opt.bots = atoi(v); i++; }
		else if (a == "--duration") { opt.duration = atof(v); i++; }
		else if (a == "--spread") { opt.spread = (float)atof(v); i++; }
		else if (a == "--rate") { opt.rate = atoi(v); i++; }
		else if (a == "--protocol") { opt.protocol = (unsigned int)atoi(v); i++; }
		else if (a == "--threads") { opt.threads = atoi(v); i++; }
		else if (a == "--center") { if (!ParseVec(v, opt.center)) { fprintf(stderr, "--center wants x,y,z\n"); return 2; } i++; }
		else if (a == "--quiet") opt.quiet = true;
		else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
	}
	if (opt.bots < 1) opt.bots = 1;
	if (opt.threads <= 0) opt.threads = opt.bots < 64 ? 1 : (opt.bots + 127) / 128;
	if (opt.threads > 32) opt.threads = 32;

	std::vector<Bot> bots(opt.bots);
	for (int i = 0; i < opt.bots; i++)
	{
		bots[i].index = i;
		// evenly over a disc of radius spread (sunflower layout), so that the
		// density is uniform and the streaming tiers get exercised
		float r = opt.spread * sqrtf((i + 0.5f) / (float)opt.bots);
		float a = (float)i * 2.399963f;   // golden angle
		bots[i].home = CVector3(opt.center.fX + r * cosf(a), opt.center.fY + r * sinf(a), opt.center.fZ);
		bots[i].phase = a;
	}

	printf("orange_bot: %d bot(s) -> %s:%u, %d threads, spread %.0f m, %d packets/s each, %.0f s\n",
		opt.bots, opt.host.c_str(), opt.port, opt.threads, opt.spread, opt.rate, opt.duration);

	std::vector<std::thread> workers;
	std::atomic<int> started(0);
	for (int t = 0; t < opt.threads; t++)
	{
		workers.emplace_back([&, t]() {
			for (int i = t; i < opt.bots; i += opt.threads)
			{
				BotStart(bots[i], opt);
				started++;
				RakSleep(1);   // spreads the connection requests a little
			}
			while (g_running)
			{
				unsigned long now = (unsigned long)RakNet::GetTimeMS();
				for (int i = t; i < opt.bots; i += opt.threads)
					BotPump(bots[i], opt, now);
				RakSleep(5);
			}
			// Leave politely: every bot sends its disconnection notification,
			// the peers are pumped for a moment so it goes out, then shut down
			// without waiting (a blocking Shutdown per bot would take minutes).
			for (int i = t; i < opt.bots; i += opt.threads)
				if (bots[i].peer && bots[i].accepted)
					bots[i].peer->CloseConnection(bots[i].server, true, 0, LOW_PRIORITY);
			for (int round = 0; round < 60; round++)
			{
				for (int i = t; i < opt.bots; i += opt.threads)
					if (bots[i].peer)
						for (RakNet::Packet * p = bots[i].peer->Receive(); p; bots[i].peer->DeallocatePacket(p), p = bots[i].peer->Receive()) {}
				RakSleep(5);
			}
			for (int i = t; i < opt.bots; i += opt.threads)
				if (bots[i].peer)
				{
					bots[i].peer->Shutdown(0);
					RakNet::RakPeerInterface::DestroyInstance(bots[i].peer);
					bots[i].peer = nullptr;
				}
		});
	}

	auto begin = std::chrono::steady_clock::now();
	unsigned long lastSnapshots = 0, lastEntries = 0;
	auto lastReport = begin;
	for (;;)
	{
		RakSleep(200);
		auto now = std::chrono::steady_clock::now();
		double elapsed = std::chrono::duration<double>(now - begin).count();
		if (std::chrono::duration<double>(now - lastReport).count() >= 1.0)
		{
			int accepted = 0, established = 0, failed = 0;
			unsigned long snapshots = 0, entries = 0;
			size_t maxSeen = 0;
			for (const Bot & b : bots)
			{
				if (b.accepted) accepted++;
				if (b.established) established++;
				if (b.failed) failed++;
				snapshots += b.snapshots;
				entries += b.entries;
				if (b.seen.size() > maxSeen) maxSeen = b.seen.size();
			}
			double dt = std::chrono::duration<double>(now - lastReport).count();
			if (!opt.quiet)
				printf("[%5.1fs] established %d/%d (failed %d), snapshot datagrams %.0f/s, player states %.0f/s, most players seen by one bot: %zu\n",
					elapsed, established, opt.bots, failed, (snapshots - lastSnapshots) / dt, (entries - lastEntries) / dt, maxSeen);
			lastSnapshots = snapshots;
			lastEntries = entries;
			lastReport = now;
		}
		if (elapsed >= opt.duration)
			break;
	}
	g_running = false;
	for (std::thread & w : workers)
		w.join();

	int established = 0, withEntries = 0, failed = 0;
	unsigned long snapshots = 0, entries = 0, infos = 0;
	size_t maxSeen = 0, minSeen = (size_t)-1;
	unsigned char maxEntries = 0;
	for (Bot & b : bots)
	{
		if (b.established) established++;
		if (b.failed) failed++;
		if (b.entries) withEntries++;
		snapshots += b.snapshots;
		entries += b.entries;
		infos += b.infos;
		if (b.seen.size() > maxSeen) maxSeen = b.seen.size();
		if (b.seen.size() < minSeen) minSeen = b.seen.size();
		if (b.maxEntries > maxEntries) maxEntries = b.maxEntries;
	}
	if (minSeen == (size_t)-1) minSeen = 0;
	printf("summary: %d/%d bots accepted by the server, %d failed; %lu snapshot datagrams with %lu player states received in total,"
		" %lu player info records; a bot saw between %zu and %zu other players, at most %u per datagram\n",
		established, opt.bots, failed, snapshots, entries, infos, minSeen, maxSeen, (unsigned)maxEntries);
	bool ok = established == opt.bots && failed == 0 && (opt.bots == 1 || withEntries == opt.bots);
	printf("RESULT: %s\n", ok ? "OK" : "FAILED");
	return ok ? 0 : 1;
}
