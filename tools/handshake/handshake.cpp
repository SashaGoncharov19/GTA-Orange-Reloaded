// orange_handshake: connects to an orange_server the way the game client
// does and reports what happens. No game needed. Shipped with the server so
// that "does the server accept players?" can be answered from any machine
// (firewall, port, IPv4 binding) before GTA V is started.
//
//   orange_handshake [host] [port] [nickname]
//
// Exit code 0: the server accepted the player (ID_CONNECT_TO_SERVER came
// back); 1: connected but not accepted, or no answer; 2: RakNet could not
// start or the connection attempt could not be started.
//
// Protocol (orange-core/Network/CNetworkConnection.cpp): after RakNet's
// ID_CONNECTION_REQUEST_ACCEPTED the client sends ID_CONNECT_TO_SERVER with
// its nickname; the server creates the player, runs the resources'
// PlayerConnect handlers and answers ID_CONNECT_TO_SERVER. What follows are
// the RPCs of the resources (ID_RPC_PLUGIN) and the sync packets.
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "RakNetTypes.h"
#include "RakString.h"
#include "RakSleep.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>

static const char * PacketName(unsigned char id)
{
	switch (id)
	{
	case ID_CONNECTION_REQUEST_ACCEPTED: return "ID_CONNECTION_REQUEST_ACCEPTED";
	case ID_CONNECTION_ATTEMPT_FAILED: return "ID_CONNECTION_ATTEMPT_FAILED";
	case ID_ALREADY_CONNECTED: return "ID_ALREADY_CONNECTED";
	case ID_NO_FREE_INCOMING_CONNECTIONS: return "ID_NO_FREE_INCOMING_CONNECTIONS";
	case ID_DISCONNECTION_NOTIFICATION: return "ID_DISCONNECTION_NOTIFICATION";
	case ID_CONNECTION_LOST: return "ID_CONNECTION_LOST";
	case ID_CONNECTION_BANNED: return "ID_CONNECTION_BANNED";
	case ID_INCOMPATIBLE_PROTOCOL_VERSION: return "ID_INCOMPATIBLE_PROTOCOL_VERSION";
	case ID_RPC_PLUGIN: return "ID_RPC_PLUGIN (resource RPC)";
	case ID_CONNECT_TO_SERVER: return "ID_CONNECT_TO_SERVER";
	case ID_SEND_PLAYER_DATA: return "ID_SEND_PLAYER_DATA";
	case ID_SEND_VEHICLE_DATA: return "ID_SEND_VEHICLE_DATA";
	case ID_CHAT_MESSAGE: return "ID_CHAT_MESSAGE";
	default: return "packet";
	}
}

int main(int argc, char ** argv)
{
	const char * host = argc > 1 ? argv[1] : "127.0.0.1";
	unsigned short port = argc > 2 ? (unsigned short)atoi(argv[2]) : 7788;
	const char * nickname = argc > 3 ? argv[3] : "handshake";
	const double totalSeconds = 12.0;   // RakNet gives up after ~6 s (12 attempts, 500 ms)
	const double afterAccept = 3.0;     // collect what the resources send

	RakNet::RakPeerInterface * client = RakNet::RakPeerInterface::GetInstance();
	RakNet::SocketDescriptor sd(0, 0);
	sd.socketFamily = AF_INET;          // the game client is IPv4 only
	RakNet::StartupResult started = client->Startup(8, &sd, 1);
	if (started != RakNet::RAKNET_STARTED)
	{
		printf("RakNet did not start (result %d)\n", (int)started);
		return 2;
	}
	RakNet::ConnectionAttemptResult attempt = client->Connect(host, port, 0, 0);
	if (attempt != RakNet::CONNECTION_ATTEMPT_STARTED)
	{
		printf("connection attempt to %s:%u could not be started (result %d)\n", host, port, (int)attempt);
		return 2;
	}
	printf("connecting to %s:%u as '%s'\n", host, port, nickname);

	bool accepted = false, established = false;
	std::map<unsigned char, int> seen;
	typedef std::chrono::steady_clock clock;
	clock::time_point start = clock::now(), establishedAt = start;
	for (;;)
	{
		clock::time_point now = clock::now();
		double secs = std::chrono::duration<double>(now - start).count();
		if (secs > totalSeconds)
			break;
		if (established && std::chrono::duration<double>(now - establishedAt).count() > afterAccept)
			break;
		for (RakNet::Packet * p = client->Receive(); p; client->DeallocatePacket(p), p = client->Receive())
		{
			unsigned char id = p->data[0];
			int times = ++seen[id];
			RakNet::BitStream in(p->data, p->length, false);
			in.IgnoreBytes(1);
			if (id == ID_CONNECTION_REQUEST_ACCEPTED)
			{
				accepted = true;
				printf("[%5.2fs] %s from %s\n", secs, PacketName(id), p->systemAddress.ToString(true));
				RakNet::BitStream out;
				out.Write((unsigned char)ID_CONNECT_TO_SERVER);
				out.Write(RakNet::RakString(nickname));
				client->Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0, p->systemAddress, false);
			}
			else if (id == ID_CONNECT_TO_SERVER)
			{
				established = true;
				establishedAt = now;
				printf("[%5.2fs] %s: the server accepted the player\n", secs, PacketName(id));
			}
			else if (id == ID_CHAT_MESSAGE)
			{
				RakNet::RakString text;
				in.Read(text);
				printf("[%5.2fs] %s: %s\n", secs, PacketName(id), text.C_String());
			}
			else if (times == 1 || id < ID_USER_PACKET_ENUM)
				printf("[%5.2fs] %s (id %u, %u bytes)\n", secs, PacketName(id), id, p->length);
		}
		RakSleep(10);
	}

	printf("packets:");
	for (std::map<unsigned char, int>::iterator it = seen.begin(); it != seen.end(); ++it)
		printf(" %s x%d,", PacketName(it->first), it->second);
	printf("\n");
	if (established)
		printf("RESULT: %s:%u accepted the player '%s'\n", host, port, nickname);
	else if (accepted)
		printf("RESULT: %s:%u answered but did not accept the player (no ID_CONNECT_TO_SERVER within %.0f s)\n", host, port, totalSeconds);
	else
		printf("RESULT: no answer from %s:%u (no server there, a firewall, or a server bound to IPv6 only)\n", host, port);
	client->Shutdown(300);
	RakNet::RakPeerInterface::DestroyInstance(client);
	return established ? 0 : 1;
}
