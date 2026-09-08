-- Example GTA:Orange Lua resource.
-- Available functions: see runtime/server/modules/lua-module/API.lua and lua-module/SResource.cpp

print("[example] resource starting")

-- Spawn a few cars near the default spawn point
local spawnCars = {
	{ "adder",   46.34,  -688.66, 43.67, 133.99 },
	{ "zentorno", 43.02, -695.12, 43.66, 159.65 },
	{ "t20",     40.28,  -702.29, 43.65, 158.81 },
}
for _, car in ipairs(spawnCars) do
	CreateVehicle(car[1], car[2], car[3], car[4], car[5])
end

-- A blip and a marker at the spawn
CreateBlipForAll(21.24, -711.04, 45.97, 1.0, 5, 1)
CreateMarkerForAll(21.24, -711.04, 44.97, 1.0, 2.0)

-- Server events (see Plugin::Trigger in Server/Plugin.cpp for the list)
OnEvent(function(name, ...)
	local args = { ... }
	if name == "PlayerConnect" then
		local playerid = args[1]
		print("[example] player " .. GetPlayerName(playerid) .. " (" .. playerid .. ") connected")
		SendPlayerMessage(playerid, "Welcome to GTA:Orange! Type /help for commands.")
		SetPlayerCoords(playerid, 21.24, -711.04, 45.97)
		GivePlayerWeapon(playerid, "WEAPON_PISTOL", 200)
	elseif name == "PlayerDisconnect" then
		print("[example] player " .. args[1] .. " left (reason " .. tostring(args[2]) .. ")")
	elseif name == "EnterMarker" then
		SendPlayerNotification(args[1], "You entered marker " .. tostring(args[2]))
	end
end)

-- Chat commands ("/car adder"). Return true to let the server reply "Unknown command".
OnCommand(function(playerid, cmd)
	local words = {}
	for w in string.gmatch(cmd, "%S+") do words[#words + 1] = w end
	local name = words[1]

	if name == "/help" then
		SendPlayerMessage(playerid, "Commands: /car <model>, /pos, /heal")
		return false
	elseif name == "/car" and words[2] then
		local x, y, z = GetPlayerCoords(playerid)
		local veh = CreateVehicle(words[2], x + 2.0, y + 2.0, z, 0.0)
		SetPlayerIntoVehicle(playerid, veh, -1)
		return false
	elseif name == "/pos" then
		local x, y, z = GetPlayerCoords(playerid)
		SendPlayerMessage(playerid, string.format("Position: %.2f %.2f %.2f", x, y, z))
		return false
	end
	return true
end)

-- Called every server tick (~200 times per second); keep it light.
OnTick(function()
end)

-- Simple HTTP endpoint: curl http://127.0.0.1:7789/
OnHTTPReq(function(method, url, query, body)
	if url == "/" then
		return "GTA:Orange server is running\n"
	end
	return nil
end)

print("[example] resource started")
