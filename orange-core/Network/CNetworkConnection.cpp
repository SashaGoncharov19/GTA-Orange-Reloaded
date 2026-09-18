#include "stdafx.h"

#ifndef ORANGE_VERSION
#define ORANGE_VERSION "dev"
#endif

CNetworkConnection *CNetworkConnection::singleInstance = nullptr;

// Interpolate a remote player over the interval its updates actually arrive
// at, within sane bounds: the server sends 20 snapshots a second to a player
// nearby, fewer to one far away.
static unsigned long InterpolationDelay(CNetworkPlayer * player)
{
	int delay = player->GetTickTime();
	if (delay < 50) delay = 50;
	else if (delay > 200) delay = 200;
	return (unsigned long)delay;
}

CNetworkConnection::CNetworkConnection()
{
	client = RakNet::RakPeerInterface::GetInstance();
}

CNetworkConnection::~CNetworkConnection()
{
	
}

CNetworkConnection * CNetworkConnection::Get()
{
	if (!singleInstance)
		singleInstance = new CNetworkConnection();
	return singleInstance;
}

bool CNetworkConnection::Connect(std::string host, unsigned short port)
{
	if (host.empty() || !port)
		return false;
	sHost = host;
	usPort = port;

	RakNet::SocketDescriptor socketDescriptor(0, 0);
	socketDescriptor.socketFamily = AF_INET;

	RakNet::StartupResult started = client->Startup(8, &socketDescriptor, 1);
	client->SetOccasionalPing(true);
	connection = client->Connect(host.c_str(), port, 0, 0);
	log_info << "Network: connecting to " << Address() << " (startup " << (int)started << ", attempt " << (int)connection << ")" << std::endl;
	if (connection != RakNet::CONNECTION_ATTEMPT_STARTED && connection != RakNet::CONNECTION_ATTEMPT_ALREADY_IN_PROGRESS)
	{
		log_error << "Network: the connection attempt to " << Address() << " could not be started (RakNet result " << (int)connection << ")" << std::endl;
		return false;
	}
	bConnected = true;
	bEstablished = false;
	CRPCPlugin::Get();
	return true;
}

void CNetworkConnection::ConnectTo(const std::string & host, unsigned short port)
{
	CNetworkPlayer::Clear();
	CNetworkVehicle::Clear();
	CNetworkObject::Clear();
	if (IsConnected())
		Disconnect();
	std::stringstream ss;
	ss << "Connecting to " << host << ":" << port;
	CChat::Get()->AddChatMessage(ss.str());
	if (!Connect(host, port))
		CChat::Get()->AddChatMessage("Can't connect to the server", { 255, 0, 0, 255 });
}

void CNetworkConnection::Disconnect()
{
	log_info << "Network: disconnecting from " << Address() << std::endl;
	bConnected = false;
	bEstablished = false;
	client->Shutdown(300);
	CChat::Get()->Clear();
	CChat::Get()->AddChatMessage("Disconnected");
	CNetworkPlayer::Clear();
}

void CNetworkConnection::Tick()
{
	for (packet = client->Receive(); packet; client->DeallocatePacket(packet), packet = client->Receive()) {
		unsigned char packetID = packet->data[0];
		RakNet::BitStream bsIn(packet->data, packet->length, false);
		RakNet::BitStream bsOut;
		bsIn.IgnoreBytes(sizeof(unsigned char));

		switch (packetID) {
			case ID_CONNECTION_REQUEST_ACCEPTED:
			{
				log_info << "Network: connection accepted by " << packet->systemAddress.ToString(true) << std::endl;
				CChat::Get()->AddChatMessage("Connected to " + Address());
				CLocalPlayer::Get()->FreezePosition(false);
				CLocalPlayer::Get()->SetVisible(true);
				RakString playerName(CConfig::Get()->sNickName.c_str());
				bsOut.Write((unsigned char)ID_CONNECT_TO_SERVER);
				bsOut.Write(playerName);
				// who we are, so that the server can serve older clients
				// the old way and log what connects
				bsOut.Write(RakString(ORANGE_VERSION));
				bsOut.Write((unsigned int)ORANGE_PROTOCOL_VERSION);
				CLocalPlayer::Get()->SetMoney(0);

				client->Send(&bsOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
				break;
			}
			case ID_CONNECTION_ATTEMPT_FAILED:
			{
				// Nothing answered on that address: no server there, a
				// firewall, or a server bound to IPv6 only (the client uses IPv4).
				log_error << "Network: no answer from " << Address() << " (is orange_server running there on UDP " << usPort << "?)" << std::endl;
				CLocalPlayer::Get()->SetMoney(0);
				CChat::Get()->AddChatMessage("Not connected: " + Address() + " did not answer. Is the server running?", { 255, 0, 0, 255 });
				bConnected = false;
				bEstablished = false;
				CGlobals::Get().displayServerBrowser = true;
				break;
			}
			case ID_NO_FREE_INCOMING_CONNECTIONS:
			{
				CLocalPlayer::Get()->SetMoney(0);
				CChat::Get()->AddChatMessage("Server is full!");
				break;
			}
			case ID_DISCONNECTION_NOTIFICATION:
			case ID_CONNECTION_LOST:
			{
				bool closed = packetID == ID_DISCONNECTION_NOTIFICATION;
				if (closed)
					log_info << "Network: the server closed the connection" << std::endl;
				else
					log_error << "Network: connection to " << Address() << " lost" << std::endl;
				CLocalPlayer::Get()->SetMoney(0);
				bEstablished = false;
				bConnected = false;
				// the others are gone with the session
				CNetworkPlayer::Clear();
				CChat::Get()->AddChatMessage(closed ? "Connection closed!" : "Connection lost!", { 255, 100, 100, 255 });
				CGlobals::Get().displayServerBrowser = true;
				break;
			}
			case ID_CONNECTION_BANNED:
			{
				CLocalPlayer::Get()->SetMoney(0);
				CChat::Get()->AddChatMessage("You are banned!");
				break;
			}
			case ID_CONNECT_TO_SERVER:
			{
				log_info << "Network: the server accepted the player, synchronisation starts" << std::endl;
				bEstablished = true;
				// Leave the lobby scene (scripted camera, hidden HUD) now;
				// until here that only happened when a resource positioned
				// the player, and a server without resources left the
				// player staring at the Vinewood panorama.
				if (!CLocalPlayer::Get()->Spawned)
					CLocalPlayer::Get()->Spawn();
				break;
			}
			case ID_PLAYER_INFO:
			{
				// who is who; a snapshot with this player can arrive before
				// or after this record (different channels)
				RakNet::RakNetGUID guid;
				unsigned int id = 0;
				RakNet::RakString name;
				RemotePlayerInfo info;
				bsIn.Read(guid);
				bsIn.Read(id);
				bsIn.Read(name);
				bsIn.Read(info.model);
				bsIn.Read(info.color);
				info.id = id;
				info.name = name.C_String();
				CNetworkPlayer::Remember(guid, info);
				if (CNetworkPlayer * player = CNetworkPlayer::GetByGUID(guid, false))
				{
					player->SetName(info.name);
					player->SetId(id);
					player->SetColor(info.color);
				}
				log_debug << "Network: player " << id << " '" << info.name << "' is online (" << CNetworkPlayer::KnownCount() << " known)" << std::endl;
				break;
			}
			case ID_PLAYER_SNAPSHOT:
			{
				// the states of the players around us, see docs/NETWORK.md
				unsigned int serverTime = 0;
				unsigned char count = 0;
				bsIn.Read(serverTime);
				bsIn.Read(count);
				for (unsigned char i = 0; i < count; i++)
				{
					RakNet::RakNetGUID guid;
					OnFootSyncData data;
					if (!bsIn.Read(guid) || !bsIn.Read(data))
						break;
					if (guid == client->GetMyGUID())
						continue;
					CNetworkPlayer *remotePlayer = CNetworkPlayer::GetByGUID(guid, false);
					if (!remotePlayer)
					{
						// first sight: the ped appears where the player is
						CNetworkPlayer::hFutureModel = data.hModel;
						CNetworkPlayer::vecFuturePosition = data.vecPos;
						remotePlayer = CNetworkPlayer::GetByGUID(guid, true);
					}
					if (!remotePlayer->AcceptServerTime(serverTime))
						continue;   // overtaken by a newer datagram
					remotePlayer->UpdateLastTickTime();
					remotePlayer->SetOnFootData(data, InterpolationDelay(remotePlayer));
					if (data.bShooting)
						remotePlayer->Interpolate();
				}
				break;
			}
			case ID_SEND_PLAYER_DATA:
			{
				// the 2017 relay (GUID, name, state), from a server that has
				// not been updated to snapshots
				OnFootSyncData data;
				RakNet::RakNetGUID playerGUID;
				RakNet::RakString rsName;
				bsIn.Read(playerGUID);
				bsIn.Read(rsName);
				bsIn.Read(data);
				if (playerGUID == client->GetMyGUID())
					break;
				CNetworkPlayer *remotePlayer = CNetworkPlayer::GetByGUID(playerGUID, false);
				if (!remotePlayer)
				{
					CNetworkPlayer::hFutureModel = data.hModel;
					CNetworkPlayer::vecFuturePosition = data.vecPos;
					remotePlayer = CNetworkPlayer::GetByGUID(playerGUID, true);
				}
				if(rsName.GetLength())
					remotePlayer->SetName(std::string(rsName.C_String()));
				remotePlayer->MarkHasState();
				remotePlayer->UpdateLastTickTime();
				remotePlayer->SetOnFootData(data, InterpolationDelay(remotePlayer));
				if (data.bShooting)
					remotePlayer->Interpolate();
				break;
			}
			case ID_SEND_VEHICLE_DATA:
			{
				VehicleData data;
				RakNet::RakNetGUID vehGUID;
				RakNet::RakString rsName;
				bsIn.Read(data);

				if (data.GUID != UNASSIGNED_RAKNET_GUID)
				{
					CNetworkVehicle *remoteVeh = CNetworkVehicle::GetByGUID(data.GUID);
					if (remoteVeh)
					{
						remoteVeh->UpdateLastTickTime();
						remoteVeh->SetVehicleData(data, 100 + (int)(data.vecMoveSpeed.Length()*1.8)); // remoteVeh->GetTickTime());
					}
				}

				break;
			}
			case ID_SEND_TASKS:
			{
				RakNet::RakNetGUID playerGUID;
				bsIn.Read(playerGUID);
				CNetworkPlayer * player = CNetworkPlayer::GetByGUID(playerGUID, false);
				std::vector<TaskPair> ClonedTasks;
				int parentTaskID = -1;
				if (player)
				{
					int tasks;
					bsIn.Read(tasks);
					for (int i = 0; i < tasks; i++)
					{
						unsigned short taskID;
						bsIn.Read(taskID);
						log_debug << "Recieved " << VTasks::Get()->GetTaskName(taskID) << std::endl;

						unsigned int size;
						bsIn.Read(size);

						int bytesSize = (size % 8) ? (size / 8 + 1) : (size / 8);
						unsigned char* taskInfo = new unsigned char[bytesSize];
						bsIn.ReadBits(taskInfo, size);
						rageBuffer data;
						typedef void(*InitBuffer)(rageBuffer*);
						typedef void(*InitReadBuffer)(rageBuffer*, unsigned char*, int, int);
						typedef CSerialisedFSMTaskInfo*(*CreateTaskInfoByID)(unsigned int);
						static InitBuffer initBuffer = GameFunc<InitBuffer>("RageBufferInit");
						static InitReadBuffer initReadBuffer = GameFunc<InitReadBuffer>("RageBufferInitRead");
						static CreateTaskInfoByID createTaskInfo = GameFunc<CreateTaskInfoByID>("CreateTaskInfoById");
						if (!initBuffer || !initReadBuffer || !createTaskInfo)
						{
							// Task synchronisation offsets are unresolved on this game build.
							delete[] taskInfo;
							continue;
						}
						initBuffer(&data);
						initReadBuffer(&data, taskInfo, size, 0);
						CSerialisedFSMTaskInfo* serTask = createTaskInfo(taskID);
						if (!serTask)
						{
							delete[] taskInfo;
							continue;
						}

						serTask->Read(&data);
						ClonedTasks.push_back({ serTask, taskID });
						delete[size] taskInfo;
						if (parentTaskID == -1)
							parentTaskID = taskID;
					}
					if (parentTaskID != -1)
					{
						GTA::CTask *parentTask = nullptr;
						GTA::CTask *cursorTask = nullptr;
						for each (auto cloned in ClonedTasks)
						{
							if (!parentTask)
							{
								parentTask = (GTA::CTask*)cloned.task->GetTask();
								parentTask->Deserialize(cloned.task);
								cursorTask = parentTask;
							}
							else
							{
								GTA::CTask *newTask = (GTA::CTask*)cloned.task->GetTask();
								newTask->Deserialize(cloned.task);
								cursorTask->NextSubTask = newTask;
								cursorTask = newTask;
							}
						}
						log_debug << "Assigned: " << parentTask->GetTree() << std::endl;
						player->AssignTask(parentTask);

						for each (auto cloned in ClonedTasks)
							rage::sysMemAllocator::Get()->free((void*)cloned.task, rage::HEAP_TASK_CLONE);
					}
				}
				break;
			}
			case ID_PLAYER_LEFT:
			{
				RakNet::RakNetGUID guid;
				bsIn.Read(guid);
				CNetworkPlayer::DeleteByGUID(guid);
				CNetworkPlayer::Forget(guid);
				break;
			}
			default:
			{
				// once per identifier, in the log: the payload is binary and
				// the chat is no place for it
				static std::set<unsigned char> said;
				if (said.insert(packetID).second)
					log_info << "Network: unknown message id " << (int)packetID << " (" << packet->length << " bytes) from the server, ignored from now on" << std::endl;
				break;
			}
		}
	}
}
