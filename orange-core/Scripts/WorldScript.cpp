#include "stdafx.h"

// The multiplayer world: no ambient peds and traffic, no police dispatch, no
// wanted level, no pause menu, no leftover story blips.
//
// The 2017 client got the same with byte patches at reference-build
// addresses (DisablePopulationPeds_*, DisablePopulationVehicles_*,
// DisableCopsAndFireTrucks_*, DisableWantedGeneration_*, EscFreeze,
// DisableNorthBlip in GameOffsets.cpp); those have no patterns on current
// builds. The natives below do it on every build, the way the other GTA V
// multiplayer clients do. With orange.storymode nothing here runs.

// Every blip the stock scripts left behind (missions, shops, safehouses):
// once, right after the take-over, before anything of ours exists. Blips are
// enumerated per sprite id.
static int RemoveAllBlips()
{
	int removed = 0;
	for (int sprite = 0; sprite < 1024; sprite++)
	{
		for (Blip blip = UI::GET_FIRST_BLIP_INFO_ID(sprite); UI::DOES_BLIP_EXIST(blip); blip = UI::GET_NEXT_BLIP_INFO_ID(sprite))
		{
			UI::REMOVE_BLIP(&blip);
			if (++removed >= 4096)
				return removed;
		}
	}
	return removed;
}

static void WorldOnce()
{
	_Player player = PLAYER::PLAYER_ID();
	Ped ped = PLAYER::PLAYER_PED_ID();

	// nothing new spawns...
	STREAMING::SET_PED_POPULATION_BUDGET(0);
	STREAMING::SET_VEHICLE_POPULATION_BUDGET(0);
	VEHICLE::SET_RANDOM_TRAINS(false);
	VEHICLE::SET_RANDOM_BOATS(false);
	VEHICLE::SET_GARBAGE_TRUCKS(false);
	VEHICLE::SET_ALL_LOW_PRIORITY_VEHICLE_GENERATORS_ACTIVE(false);
	VEHICLE::SET_ALL_VEHICLE_GENERATORS_ACTIVE_IN_AREA(-16000.f, -16000.f, -2000.f, 16000.f, 16000.f, 2000.f, false, false);
	PED::SET_CREATE_RANDOM_COPS(false);
	PED::SET_CREATE_RANDOM_COPS_NOT_ON_SCENARIOS(false);
	PED::SET_CREATE_RANDOM_COPS_ON_SCENARIOS(false);
	PLAYER::SET_DISPATCH_COPS_FOR_PLAYER(player, false);
	PLAYER::SET_POLICE_IGNORE_PLAYER(player, true);
	PLAYER::SET_MAX_WANTED_LEVEL(0);
	PLAYER::SET_WANTED_LEVEL_MULTIPLIER(0.f);

	// ...and what the single player world had spawned goes
	Vector3 pos = ENTITY::GET_ENTITY_COORDS(ped, true);
	GAMEPLAY::CLEAR_AREA_OF_PEDS(pos.x, pos.y, pos.z, 12000.f, 0);
	GAMEPLAY::CLEAR_AREA_OF_VEHICLES(pos.x, pos.y, pos.z, 12000.f, false, false, false, false, false);
	int blips = RemoveAllBlips();
	log_info << "World: ambient peds and traffic, police dispatch and the wanted level are off, " << blips << " leftover blip(s) removed" << std::endl;
}

void WorldAction()
{
	if (CGlobals::Get().storyMode)
		for (;;)
			scriptWait(1000);

	// GameInit teleports the player on foot in its first ticks; the clearing
	// below must not catch the car the story left the player in.
	scriptWait(500);
	WorldOnce();
	for (;;)
	{
		PED::SET_PED_DENSITY_MULTIPLIER_THIS_FRAME(0.f);
		PED::SET_SCENARIO_PED_DENSITY_MULTIPLIER_THIS_FRAME(0.f, 0.f);
		VEHICLE::SET_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME(0.f);
		VEHICLE::SET_RANDOM_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME(0.f);
		VEHICLE::SET_PARKED_VEHICLE_DENSITY_MULTIPLIER_THIS_FRAME(0.f);
		// The pause menu freezes a single player game (a network session of
		// the game would not; GTA:Orange has none yet), so the frontend stays
		// closed: Escape belongs to the chat, /quit leaves the game.
		UI::DISABLE_FRONTEND_THIS_FRAME();
		scriptWait(0);
	}
}

SCRIPT(WorldAction);
