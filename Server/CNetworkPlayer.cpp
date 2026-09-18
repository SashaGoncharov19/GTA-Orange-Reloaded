#include "stdafx.h"

std::vector<CNetworkPlayer*> CNetworkPlayer::_players;
std::unordered_map<uint64_t, CNetworkPlayer*> CNetworkPlayer::_byGuid;
unsigned int CNetworkPlayer::_legacyPlayers = 0;

void CNetworkPlayer::Each(void(*func)(CNetworkPlayer *))
{
	for (CNetworkPlayer *player : _players)
		if (player)
			func(player);
}

CNetworkPlayer * CNetworkPlayer::Create(RakNet::RakNetGUID GUID, const RakNet::SystemAddress & address)
{
	CNetworkPlayer * player = new CNetworkPlayer(GUID);
	player->rnAddress = address;
	return player;
}

CNetworkPlayer * CNetworkPlayer::GetByGUID(RakNet::RakNetGUID GUID)
{
	auto it = _byGuid.find(GUID.g);
	return it == _byGuid.end() ? nullptr : it->second;
}

CNetworkPlayer * CNetworkPlayer::GetByID(UINT playerID)
{
	if (playerID < _players.size())
		return _players[playerID];
	return nullptr;
}

void CNetworkPlayer::AddPlayer(CNetworkPlayer *player)
{
	int playerID = -1;
	for (size_t i = 0; i < _players.size(); ++i)
	{
		if (!_players[i])
		{
			playerID = (int)i;
			break;
		}
	}
	if (playerID == -1)
	{
		player->uiID = (unsigned int)_players.size();
		_players.push_back(player);
	}
	else
	{
		player->uiID = (unsigned int)playerID;
		_players[playerID] = player;
	}
	_byGuid[player->rnGUID.g] = player;
}

CNetworkPlayer::CNetworkPlayer(RakNet::RakNetGUID GUID):rnGUID(GUID)
{
	memset(&lastSync, 0, sizeof(lastSync));
	lastSync.rnVehicle = UNASSIGNED_RAKNET_GUID;
	colColor = { 0xFF, 0x8F, 0x00, 0xFF };
	AddPlayer(this);
}

CNetworkPlayer::~CNetworkPlayer()
{
	if (IsLegacy() && _legacyPlayers) _legacyPlayers--;
	if (uiID < _players.size() && _players[uiID] == this)
		_players[uiID] = nullptr;
	auto it = _byGuid.find(rnGUID.g);
	if (it != _byGuid.end() && it->second == this)
		_byGuid.erase(it);
}

bool CNetworkPlayer::AcceptSync(unsigned long nowMs, unsigned int maxRateHz)
{
	if (maxRateHz)
	{
		unsigned long minInterval = 1000 / maxRateHz;
		// a little slack: clients time their sends by frames
		if (ulLastSyncMs && nowMs - ulLastSyncMs < minInterval / 2)
		{
			ulSyncDropped++;
			return false;
		}
	}
	ulLastSyncMs = nowMs;
	ulSyncPackets++;
	return true;
}

void CNetworkPlayer::SetOnFootData(const OnFootSyncData& data)
{
	lastSync = data;
	bHasPosition = true;
	hModel = data.hModel;
	bJumping = data.bJumping;
	fMoveSpeed = data.fMoveSpeed;
	vecPosition = data.vecPos;
	vecRotation = data.vecRot;
	fHeading = data.fHeading;
	ulWeapon = data.ulWeapon;
	uAmmo = data.uAmmo;
	usHealth = data.usHealth;
	usArmour = data.usArmour;
	bDucking = data.bDuckState;
	vecMoveSpeed = data.vecMoveSpeed;
	vecAim = data.vecAim;
	bAiming = data.bAiming;
	bShooting = data.bShooting;
	bInVehicle = data.bInVehicle;

	if (!bInVehicle && bEnteringVeh)
	{
		Plugin::Trigger("LeftVehicle", (unsigned long)GetID(), RakNetGUID::ToUint32(vehicle));
		bEnteringVeh = false;
	}
	else if (bInVehicle && !bEnteringVeh) {
		Plugin::Trigger("EnterVehicle", (unsigned long)GetID(), RakNetGUID::ToUint32(data.rnVehicle));
		bEnteringVeh = true;
	}

	vehicle = data.rnVehicle;
	cSeat = data.cSeat;
}

void CNetworkPlayer::GetOnFootData(OnFootSyncData& data)
{
	data = lastSync;
}

void CNetworkPlayer::SetPosition(const CVector3 & position)
{
	// The client answers with its next on-foot packet; until then the
	// server-side position is what the script asked for, so that a resource
	// reading it right after the call sees its own value.
	vecPosition = position;
	lastSync.vecPos = position;
	RakNet::BitStream bsOut;
	bsOut.Write(position);
	CRPCPlugin::Get()->Signal("SetPlayerPos", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, rnGUID, false, false);
}

void CNetworkPlayer::SetCoords(const CVector3 & position)
{
	vecPosition = position;
	lastSync.vecPos = position;
	bHasPosition = true;
}

void CNetworkPlayer::SetHeading(float heading)
{
	fHeading = heading;
	RakNet::BitStream bsOut;
	bsOut.Write(heading);
	CRPCPlugin::Get()->Signal("SetPlayerHeading", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, rnGUID, false, false);
}

void CNetworkPlayer::GiveWeapon(unsigned int weaponHash, unsigned int ammo)
{
	RakNet::BitStream bsOut;
	bsOut.Write(weaponHash);
	bsOut.Write(ammo);
	CRPCPlugin::Get()->Signal("GivePlayerWeapon", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, rnGUID, false, false);
}

void CNetworkPlayer::GiveAmmo(unsigned int weaponHash, unsigned int ammo)
{
	RakNet::BitStream bsOut;
	bsOut.Write(weaponHash);
	bsOut.Write(ammo);
	CRPCPlugin::Get()->Signal("GivePlayerAmmo", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, rnGUID, false, false);
}

void CNetworkPlayer::SetModel(unsigned int model)
{
	hModel = model;
	RakNet::BitStream bsOut;
	bsOut.Write(model);
	CRPCPlugin::Get()->Signal("SetPlayerModel", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, rnGUID, false, false);
}

void CNetworkPlayer::SetHealth(float health)
{
	RakNet::BitStream bsOut;
	bsOut.Write(health);
	CRPCPlugin::Get()->Signal("SetPlayerHealth", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, rnGUID, false, false);
}

void CNetworkPlayer::SetArmour(float armour)
{
	RakNet::BitStream bsOut;
	bsOut.Write(armour);
	CRPCPlugin::Get()->Signal("SetPlayerArmour", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, rnGUID, false, false);
}

void CNetworkPlayer::SetColor(unsigned int color)
{
	RakNet::BitStream bsOut;
	color_t col;
	col.red = (BYTE)((color >> 24) & 0xFF);
	col.green = (BYTE)((color >> 16) & 0xFF);
	col.blue = (BYTE)((color >> 8) & 0xFF);
	col.alpha = (BYTE)(color & 0xFF);
	bsOut.Write(col);
	colColor = col;
	CRPCPlugin::Get()->Signal("SetPlayerColor", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, rnGUID, false, false);
}

void CNetworkPlayer::Tick()
{
	// Death and respawn, from what the client reports: a GTA V ped with
	// health at or below 100 is dead (the fatal threshold), 0 once removed.
	for (CNetworkPlayer *player : _players)
	{
		if (!player || !player->bHasPosition)
			continue;
		if (player->usHealth <= 100 && !player->bDead)
		{
			player->bDead = true;
			Plugin::Trigger("PlayerDeath", (unsigned long)player->GetID());
		}
		else if (player->usHealth > 100 && player->bDead)
		{
			player->bDead = false;
			Plugin::Trigger("PlayerRespawn", (unsigned long)player->GetID());
		}
	}
}

UINT CNetworkPlayer::Count()
{
	return (UINT)_byGuid.size();
}

const std::vector<CNetworkPlayer *> & CNetworkPlayer::All()
{
	return _players;
}

void CNetworkPlayer::Remove(int playerid)
{
	if (playerid < 0 || (size_t)playerid >= _players.size() || !_players[playerid])
		return;
	delete _players[playerid];   // the destructor clears the slot and the map entry
}

void CNetworkPlayer::SetProtocol(unsigned int protocol)
{
	if (IsLegacy() && _legacyPlayers) _legacyPlayers--;
	uProtocol = protocol;
	if (IsLegacy()) _legacyPlayers++;
}
