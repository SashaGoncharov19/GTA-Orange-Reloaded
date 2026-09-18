#pragma once

//Main
int lua_print(lua_State *L);
int lua_tick(lua_State *L);
int lua_HTTPReq(lua_State * L);
int lua_Event(lua_State *L);
int lua_Command(lua_State *L);
int lua_LoadClientScript(lua_State *L);

int lua_SetTimer(lua_State *L);
int lua_SetInterval(lua_State *L);
int lua_ClearTimer(lua_State *L);
int lua_GetServerTime(lua_State *L);

//Player
int lua_GetPlayerCoords(lua_State *L);
int lua_GetPlayerModel(lua_State *L);
int lua_SetPlayerModel(lua_State *L);
int lua_GetPlayerName(lua_State *L);
int lua_SetPlayerName(lua_State *L);
int lua_GivePlayerWeapon(lua_State *L);
int lua_GivePlayerAmmo(lua_State *L);
int lua_SetPlayerCoords(lua_State *L);
int lua_SendPlayerNotification(lua_State *L);
int lua_SetPlayerInfoMsg(lua_State *L);
int lua_SendPlayerMessage(lua_State *L);
int lua_SendMessageToAll(lua_State *L);
int lua_PlayerExists(lua_State *L);
int lua_SetPlayerIntoVehicle(lua_State *L);
int lua_GetPlayerHealth(lua_State *L);
int lua_SetPlayerHealth(lua_State *L);
int lua_GetPlayerArmour(lua_State *L);
int lua_SetPlayerArmour(lua_State *L);
int lua_GetPlayerHeading(lua_State *L);
int lua_SetPlayerHeading(lua_State *L);
int lua_GetPlayerWeapon(lua_State *L);
int lua_IsPlayerInVehicle(lua_State *L);
int lua_GetPlayerVehicle(lua_State *L);
int lua_GetPlayerSeat(lua_State *L);
int lua_GetPlayerPing(lua_State *L);
int lua_GetPlayerAddress(lua_State *L);
int lua_GetPlayerClientVersion(lua_State *L);
int lua_IsPlayerDead(lua_State *L);
int lua_IsPlayerInRange(lua_State *L);
int lua_KickPlayer(lua_State *L);
int lua_GetPlayerMoney(lua_State *L);
int lua_SetPlayerMoney(lua_State *L);
int lua_GivePlayerMoney(lua_State *L);
int lua_SetPlayerColor(lua_State *L);
int lua_GetPlayerColor(lua_State *L);
int lua_GetPlayers(lua_State *L);
int lua_GetPlayerCount(lua_State *L);
int lua_GetMaxPlayers(lua_State *L);

//Vehicle
int lua_CreateVehicle(lua_State *L);
int lua_DeleteVehicle(lua_State *L);
int lua_VehicleExists(lua_State *L);
int lua_GetVehicleCoords(lua_State *L);
int lua_SetVehicleCoords(lua_State *L);
int lua_GetVehicleRotation(lua_State *L);
int lua_GetVehicleHealth(lua_State *L);
int lua_GetVehicleModel(lua_State *L);
int lua_GetVehicleDriver(lua_State *L);
int lua_GetVehicles(lua_State *L);

//Object
int lua_CreateObject(lua_State *L);

//Blips
int lua_CreateBlipForAll(lua_State *L);
int lua_CreateBlipForPlayer(lua_State *L);
int lua_DeleteBlip(lua_State *L);
int lua_SetBlipColor(lua_State *L);
int lua_SetBlipRoute(lua_State *L);

//Markers
int lua_CreateMarkerForAll(lua_State *L);
int lua_CreateMarkerForPlayer(lua_State *L);
int lua_DeleteMarker(lua_State *L);

//MySQL
int luaopen_luasql_mysql(lua_State *L);

//3DText
int lua_Create3DText(lua_State *L);
int lua_Set3DTextText(lua_State *L);
int lua_Attach3DTextToVeh(lua_State *L);
int lua_Attach3DTextToPlayer(lua_State *L);
int lua_Delete3DText(lua_State *L);
