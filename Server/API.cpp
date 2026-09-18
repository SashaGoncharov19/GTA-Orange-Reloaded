#include "stdafx.h"

API * API::instance = nullptr;

void API::LoadClientScript(std::string name, char* buffer, size_t size)
{
	CClientScripting::AddScript(name, buffer, size);
}

bool API::SetPlayerPosition(long playerid, float x, float y, float z)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	player->SetPosition(CVector3(x, y, z));
	return true;
}

CVector3 API::GetPlayerPosition(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return CVector3(0.f, 0.f, 0.f);
	CVector3 vecPos;
	player->GetPosition(vecPos);
	return vecPos;
}

bool API::IsPlayerInRange(long playerid, float x, float y, float z, float range)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	CVector3 vecPos, vecNextPos(x, y, z);
	player->GetPosition(vecPos);
	if ((vecNextPos - vecPos).Length() <= range)
		return true;
	return false;
}

bool API::SetPlayerHeading(long playerid, float angle)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	player->SetHeading(angle);
	return true;
}

bool API::GivePlayerWeapon(long playerid, long weapon, long ammo)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	player->GiveWeapon(weapon, ammo);
	return true;
}

bool API::GivePlayerAmmo(long playerid, long weapon, long ammo)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	player->GiveAmmo(weapon, ammo);
	return true;
}

bool API::GivePlayerMoney(long playerid, long money)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	player->GiveMoney(money);
	return true;
}

bool API::SetPlayerMoney(long playerid, long money)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	player->SetMoney(money);
	return true;
}

bool API::ResetPlayerMoney(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	player->SetMoney(0);
	return true;
}

size_t API::GetPlayerMoney(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return 0L;
	size_t money = player->GetMoney();
	return money;
}

bool API::SetPlayerModel(long playerid, long model)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	player->SetModel(model);
	return true;
}

long API::GetPlayerModel(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	return player->GetModel();
}

bool API::SetPlayerName(long playerid, const char * name)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	player->SetName(name);
	return true;
}

std::string API::GetPlayerName(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return "";
	return player->GetName();
}

bool API::SetPlayerHealth(long playerid, float health)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	player->SetHealth(health);
	return true;
}

float API::GetPlayerHealth(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return 0.f;
	return player->GetHealth();
}

bool API::SetPlayerArmour(long playerid, float armour)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	player->SetArmour(armour);
	return true;
}

float API::GetPlayerArmour(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return 0.f;
	return player->GetArmour();
}

bool API::SetPlayerColor(long playerid, unsigned int color)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	player->SetColor(color);
	return true;
}

unsigned int API::GetPlayerColor(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	color_t playerColor = player->GetColor();
	return ((playerColor.red & 0xff) << 24) + ((playerColor.green & 0xff) << 16) + ((playerColor.blue & 0xff) << 8) + (playerColor.alpha & 0xff);
}



void API::BroadcastClientMessage(const char * message, unsigned int color)
{
	RakNet::BitStream bsOut;
	RakNet::RakString msg(message);
	bsOut.Write(msg);
	color_t col;
	col.red = (BYTE)((color >> 24) & 0xFF);  // Extract the RR byte
	col.green = (BYTE)((color >> 16) & 0xFF);   // Extract the GG byte
	col.blue = (BYTE)((color >> 8) & 0xFF);   // Extract the GG byte
	col.alpha = (BYTE)((color) & 0xFF);        // Extract the BB byte
	bsOut.Write(col);
	CRPCPlugin::Get()->Signal("SendClientMessage", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true, false);
}

bool API::SendClientMessage(long playerid, const char * message, unsigned int color)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	RakNet::BitStream bsOut;
	RakNet::RakString msg(message);
	bsOut.Write(msg);
	color_t col;
	col.red = (BYTE)((color >> 24) & 0xFF);  // Extract the RR byte
	col.green = (BYTE)((color >> 16) & 0xFF);   // Extract the GG byte
	col.blue = (BYTE)((color >> 8) & 0xFF);   // Extract the GG byte
	col.alpha = (BYTE)((color) & 0xFF);        // Extract the BB byte
	bsOut.Write(col);
	CRPCPlugin::Get()->Signal("SendClientMessage", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, player->GetGUID(), false, false);
	return true;
}

bool API::SetPlayerIntoVehicle(long playerid, unsigned long vehicle, char seat)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	RakNet::BitStream bsOut;
	bsOut.Write(player->GetGUID());
	bsOut.Write(RakNetGUID(vehicle));
	bsOut.Write(seat);
	CRPCPlugin::Get()->Signal("SetPlayerIntoVehicle", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true, false);
	return true;
}

unsigned long API::CreateVehicle(long hash, float x, float y, float z, float heading)
{
	CNetworkVehicle *veh = new CNetworkVehicle(hash, x, y, z, heading);
	return RakNetGUID::ToUint32(veh->GetGUID()); // (new CNetworkVehicle(hash, x, y, z, heading));
}

bool API::SetVehiclePosition(unsigned long vehicle, float x, float y, float z)
{
	auto veh = CNetworkVehicle::GetByGUID(RakNetGUID(vehicle));
	if (!veh)
		return false;
	veh->SetPosition(CVector3(x, y, z));
	RakNet::BitStream bsOut;
	bsOut.Write(veh->GetGUID());
	bsOut.Write(CVector3(x, y, z));
	CRPCPlugin::Get()->Signal("SetVehiclePos", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true, false);
	return true;
}

CVector3 API::GetVehiclePosition(unsigned long vehicle)
{
	auto veh = CNetworkVehicle::GetByGUID(RakNetGUID(vehicle));
	if (!veh)
		return CVector3(0, 0, 0);
	return veh->GetPosition();
}

bool API::DeleteVehicle(unsigned long guid)
{
	RakNetGUID _guid(guid);
	auto veh = CNetworkVehicle::GetByGUID(_guid);
	if (veh) {
		delete veh;
	}
	BitStream bsOut;
	bsOut.Write(_guid);
	CRPCPlugin::Get()->Signal("DeleteVehicle", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true, false);
	return true;
}

bool API::CreatePickup(int type, float x, float y, float z, float scale)
{
	log << "Not implemented" << std::endl;
	return true;
}

unsigned long API::CreateBlipForAll(float x, float y, float z, float scale, int color, int sprite)
{
	CNetworkBlip * blip = new CNetworkBlip(x, y, z, scale, color, sprite, -1);
	return RakNetGUID::ToUint32(blip->rnGUID);
}

unsigned long API::CreateBlipForPlayer(long playerid, float x, float y, float z, float scale, int color, int sprite)
{
	CNetworkBlip * blip = new CNetworkBlip(x, y, z, scale, color, sprite, playerid);
	return RakNetGUID::ToUint32(blip->rnGUID);
}

void API::DeleteBlip(unsigned long guid)
{
	CNetworkBlip::GetByGUID(RakNetGUID(guid))->~CNetworkBlip();
}

void API::SetBlipColor(unsigned long _guid, int color)
{
	RakNet::BitStream bsOut;
	RakNetGUID guid = RakNetGUID(_guid);
	CNetworkBlip *blip = CNetworkBlip::GetByGUID(guid);
	blip->SetColor(color);

	bsOut.Write(guid);
	bsOut.Write(color);

	if (blip->GetPlayerID() == -1) CRPCPlugin::Get()->Signal("SetBlipColor", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true, false);
	else CRPCPlugin::Get()->Signal("SetBlipColor", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, CNetworkPlayer::GetByID(blip->GetPlayerID())->GetGUID(), false, false);
}

void API::SetBlipScale(unsigned long _guid, float scale)
{
	RakNet::BitStream bsOut;
	RakNetGUID guid = RakNetGUID(_guid);
	CNetworkBlip *blip = CNetworkBlip::GetByGUID(guid);
	blip->SetScale(scale);

	bsOut.Write(guid);
	bsOut.Write(scale);

	if(blip->GetPlayerID() == -1) CRPCPlugin::Get()->Signal("SetBlipScale", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true, false);
	else CRPCPlugin::Get()->Signal("SetBlipScale", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, CNetworkPlayer::GetByID(blip->GetPlayerID())->GetGUID(), false, false);
}

void API::SetBlipRoute(unsigned long _guid, bool route)
{
	RakNet::BitStream bsOut;
	RakNetGUID guid = RakNetGUID(_guid);
	CNetworkBlip *blip = CNetworkBlip::GetByGUID(guid);
	//blip->SetRoute(color);

	bsOut.Write(guid);
	bsOut.Write(route);

	if (blip->GetPlayerID() == -1) CRPCPlugin::Get()->Signal("SetBlipRoute", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true, false);
	else CRPCPlugin::Get()->Signal("SetBlipRoute", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, CNetworkPlayer::GetByID(blip->GetPlayerID())->GetGUID(), false, false);
}

unsigned long API::CreateMarkerForAll(float x, float y, float z, float height, float radius)
{
	CNetworkMarker * marker = new CNetworkMarker(x, y, z, height, radius, -1);
	return RakNetGUID::ToUint32(marker->rnGUID);
}

unsigned long API::CreateMarkerForPlayer(long playerid, float x, float y, float z, float height, float radius)
{
	CNetworkMarker * marker = new CNetworkMarker(x, y, z, height, radius, playerid);
	return RakNetGUID::ToUint32(marker->rnGUID);
}

void API::DeleteMarker(unsigned long guid)
{
	CNetworkMarker::GetByGUID(RakNetGUID(guid))->~CNetworkMarker();
}

unsigned long API::CreateObject(long model, float x, float y, float z, float pitch, float yaw, float roll)
{
	CNetworkObject *obj = new CNetworkObject(model, x, y, z, pitch, yaw, roll);
	return RakNetGUID::ToUint32(obj->rnGUID);
}

bool API::SendNotification(long playerid, const char * msg)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;

	RakNet::BitStream bsOut;
	bsOut.Write(RakNet::RakString(msg));

	CRPCPlugin::Get()->Signal("SendNotification", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, player->GetGUID(), false, false);
	return false;
}

bool API::SetInfoMsg(long playerid, const char* msg)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;

	RakNet::BitStream bsOut;
	bsOut.Write(true);
	bsOut.Write(RakNet::RakString(msg));

	CRPCPlugin::Get()->Signal("SetInfoMsg", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, player->GetGUID(), false, false);
	return true;
}

bool API::UnsetInfoMsg(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;

	RakNet::BitStream bsOut;
	bsOut.Write(false);

	CRPCPlugin::Get()->Signal("SetInfoMsg", &bsOut, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, player->GetGUID(), false, false);
	return true;
}

unsigned long API::Create3DText(const char * text, float x, float y, float z, int color, int outColor, float fontSize)
{
	CNetwork3DText * blip = new CNetwork3DText(x, y, z, color, outColor, text, -1, 0.f, 0.f, 0.f, fontSize);
	return RakNetGUID::ToUint32(blip->rnGUID);
}

unsigned long API::Create3DTextForPlayer(unsigned long player, const char * text, float x, float y, float z, int color, int outColor)
{
	auto pl = CNetworkPlayer::GetByGUID(RakNetGUID(player));
	if (!pl)
		return 0;
	CNetwork3DText * blip = new CNetwork3DText(x, y, z, color, outColor, text, pl->GetID());
	return RakNetGUID::ToUint32(blip->rnGUID);
}

bool API::Attach3DTextToVehicle(unsigned long textId, unsigned long vehicle, float oX, float oY, float oZ)
{
	CNetwork3DText *text = CNetwork3DText::GetByGUID(RakNetGUID(textId));
	auto veh = CNetworkVehicle::GetByGUID(RakNetGUID(vehicle));
	if (veh)
	{
		text->AttachToVehicle(*veh, oX, oY, oZ);
		return true;
	}
	return false;
}

bool API::Attach3DTextToPlayer(unsigned long textId, unsigned long player, float oX, float oY, float oZ)
{
	CNetwork3DText *text = CNetwork3DText::GetByGUID(RakNetGUID(textId));
	auto pl = CNetworkPlayer::GetByGUID(RakNetGUID(player));
	if (pl)
	{
		text->AttachToPlayer(*pl, oX, oY, oZ);
		return true;
	}
	return false;
}

bool API::Set3DTextContent(unsigned long textId, const char * text)
{
	CNetwork3DText *textItem = CNetwork3DText::GetByGUID(RakNetGUID(textId));
	textItem->SetText(text);
	return true;
}

bool API::Delete3DText(unsigned long textId)
{
	CNetwork3DText *textItem = CNetwork3DText::GetByGUID(RakNetGUID(textId));
	delete[] textItem;
	return true;
}

void API::Print(const char * message)
{
	log << message << std::endl;
}

long API::Hash(const char * str)
{
	unsigned int value = 0, temp = 0;
	for (size_t i = 0; i<strlen(str); i++)
	{
		temp = tolower(str[i]) + value;
		value = temp << 10;
		temp += value;
		value = temp >> 6;
		value ^= temp;
	}
	temp = value << 3;
	temp += value;
	unsigned int temp2 = temp >> 11;
	temp = temp2 ^ temp;
	temp2 = temp << 15;
	value = temp2 + temp;
	if (value < 2) value += 2;
	return value;
}


// ---------------------------------------------------------------------------
// 2026 additions
// ---------------------------------------------------------------------------

bool API::PlayerExists(long playerid)
{
	return playerid >= 0 && CNetworkPlayer::GetByID(playerid) != nullptr;
}

long API::GetPlayerCount()
{
	return (long)CNetworkPlayer::Count();
}

long API::GetMaxPlayers()
{
	return CConfig::Get()->MaxPlayers;
}

std::vector<long> API::GetPlayers()
{
	std::vector<long> ids;
	for (CNetworkPlayer * player : CNetworkPlayer::All())
		if (player)
			ids.push_back((long)player->GetID());
	return ids;
}

float API::GetPlayerHeading(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	return player ? player->GetHeading() : 0.f;
}

bool API::IsPlayerInVehicle(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	return player && player->bInVehicle;
}

unsigned long API::GetPlayerVehicle(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player || !player->bInVehicle || player->vehicle == UNASSIGNED_RAKNET_GUID)
		return 0;
	return RakNetGUID::ToUint32(player->vehicle);
}

int API::GetPlayerSeat(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player || !player->bInVehicle)
		return -2;
	return player->cSeat;
}

int API::GetPlayerPing(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return -1;
	return CNetworkConnection::Get()->server->GetAveragePing(player->GetGUID());
}

std::string API::GetPlayerAddress(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return "";
	return player->GetAddress().ToString(true);
}

std::string API::GetPlayerClientVersion(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	return player ? player->GetClientVersion() : "";
}

long API::GetPlayerWeapon(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	return player ? (long)player->GetWeapon() : 0;
}

bool API::IsPlayerDead(long playerid)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	return player && player->IsDead();
}

bool API::KickPlayer(long playerid, const char * reason)
{
	auto player = CNetworkPlayer::GetByID(playerid);
	if (!player)
		return false;
	CNetworkConnection::Get()->Kick(player, reason ? reason : "kicked");
	return true;
}

bool API::VehicleExists(unsigned long vehicle)
{
	return CNetworkVehicle::GetByGUID(RakNetGUID(vehicle)) != nullptr;
}

std::vector<unsigned long> API::GetVehicles()
{
	std::vector<unsigned long> ids;
	for (CNetworkVehicle * veh : CNetworkVehicle::All())
		if (veh)
			ids.push_back(RakNetGUID::ToUint32(veh->GetGUID()));
	return ids;
}

long API::GetVehicleModel(unsigned long vehicle)
{
	auto veh = CNetworkVehicle::GetByGUID(RakNetGUID(vehicle));
	return veh ? (long)veh->hashModel : 0;
}

long API::GetVehicleDriver(unsigned long vehicle)
{
	auto veh = CNetworkVehicle::GetByGUID(RakNetGUID(vehicle));
	if (!veh || !veh->hasDriver)
		return -1;
	auto player = CNetworkPlayer::GetByGUID(veh->driverGUID);
	return player ? (long)player->GetID() : -1;
}

CVector3 API::GetVehicleRotation(unsigned long vehicle)
{
	auto veh = CNetworkVehicle::GetByGUID(RakNetGUID(vehicle));
	return veh ? veh->vecRot : CVector3(0, 0, 0);
}

float API::GetVehicleHealth(unsigned long vehicle)
{
	auto veh = CNetworkVehicle::GetByGUID(RakNetGUID(vehicle));
	return veh ? (float)veh->usHealth : 0.f;
}

unsigned long API::GetServerTimeMs()
{
	return (unsigned long)RakNet::GetTimeMS();
}
