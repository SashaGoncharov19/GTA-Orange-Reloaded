#pragma once
class CNetworkConnection
{
	static CNetworkConnection *singleInstance;

	CNetworkConnection();
	bool bConnected = false;
	bool bEstablished = false;
	std::string sHost;
	unsigned short usPort = 0;
public:
	~CNetworkConnection();
	static CNetworkConnection * Get();

	bool Connect(std::string host, unsigned short port);
	// The whole cycle, to be run on the script thread (remote entities are
	// deleted through natives): drop every remote entity, disconnect, connect
	// and tell the chat what happens.
	void ConnectTo(const std::string & host, unsigned short port);
	void Disconnect();
	bool IsConnected() { return bConnected; }
	bool IsConnectionEstablished() { return bEstablished; }
	std::string Address() { return sHost + ":" + std::to_string(usPort); }
	void Tick();

	RakNet::RakPeerInterface *client;
	RakNet::Packet* packet;
	RakNet::SystemAddress clientID;
	RakNet::ConnectionAttemptResult connection;
};

