# Server scripting (Lua)

A game mode is a resource: a folder under `resources/` with a `resource.yml`
(`type: lua`) and a `main.lua`, listed under `resources:` in `config.yml`.
`lua-module` runs it on LuaJIT; `resources/example/` is a commented start.
The functions below are globals (and members of the `orange` table);
`modules/lua-module/API.lua` also defines `RGBA(r, g, b, a)` for colours.

Ids: a **player id** is a small integer (0 and up) that the server reuses
after the player leaves; a **vehicle, blip, marker, object or 3D text id** is
the number the create function returned. Positions are in metres in the
game's world coordinates; headings and rotations in degrees.

## Callbacks

| function | when |
|---|---|
| `OnEvent(fn(name, ...))` | a server event (table below) |
| `OnCommand(fn(playerid, text))` | a chat line starting with `/`; `text` is the whole line. Return `true` to have the server answer "Unknown command", `false` or nothing when handled |
| `OnTick(fn())` | every network loop iteration, up to 200 times a second: keep it cheap, prefer timers |
| `OnHTTPReq(fn(method, url, query, body))` | a request to the HTTP port; return the response body as a string, or nothing for an empty reply |
| `__OnKeyStateChanged(playerid, keycode, isUp)` | a key a client script reports; define the global function to use it |

### Events

| event | arguments | notes |
|---|---|---|
| `PlayerConnect` | `playerid` | the player is created and can be positioned; the game client is still loading |
| `PlayerDisconnect` | `playerid, reason` | `1` left, `2` connection lost; fired **before** removal, so `GetPlayerName` and friends still work inside the handler |
| `PlayerDeath` | `playerid` | health reported at 100 or below |
| `PlayerRespawn` | `playerid` | health above 100 again |
| `EnterVehicle` | `playerid, vehicle` | |
| `LeftVehicle` | `playerid, vehicle` | |
| `EnterMarker` | `playerid, marker` | |
| `LeftMarker` | `playerid, marker` | |
| `VehEnterMarker` | `vehicle, marker` | |
| `VehLeftMarker` | `vehicle, marker` | |
| `keyPress` | `playerid, keycode` | virtual key code from a client script |
| `serverEvent` | `name, ...` | raised by a client script with `__trigger(name, ...)`; booleans, numbers and strings pass through |

```lua
OnEvent(function(name, ...)
	local a = { ... }
	if name == "PlayerConnect" then
		SetPlayerCoords(a[1], 21.24, -711.04, 45.97)
	elseif name == "PlayerDeath" then
		local id = a[1]
		SetTimer(5000, function()
			if PlayerExists(id) then SetPlayerHealth(id, 200) SetPlayerCoords(id, 21.24, -711.04, 45.97) end
		end)
	end
end)
```

## Timers

| function | returns |
|---|---|
| `SetTimer(ms, fn)` | id; `fn` runs once after `ms` milliseconds |
| `SetInterval(ms, fn)` | id; `fn` runs every `ms` milliseconds until cleared |
| `ClearTimer(id)` | `true` when the timer existed |
| `GetServerTime()` | milliseconds since the server started |

Timers run on the network loop, so a callback that takes long delays
everything else; errors inside are logged as `[LUA] timer: ...`.

## Players

| function | returns / effect |
|---|---|
| `GetPlayers()` | array of the ids online |
| `GetPlayerCount()`, `GetMaxPlayers()` | numbers |
| `PlayerExists(id)` | `true` while the player is online |
| `GetPlayerName(id)` | string, empty for an unknown id |
| `SetPlayerName(id, name)` | `true` on success |
| `GetPlayerCoords(id)` | `x, y, z` |
| `SetPlayerCoords(id, x, y, z)` | teleports; `true` on success |
| `GetPlayerHeading(id)`, `SetPlayerHeading(id, degrees)` | |
| `IsPlayerInRange(id, x, y, z, range)` | `true` when within `range` metres |
| `GetPlayerModel(id)`, `SetPlayerModel(id, model)` | model as hash or name (`"a_m_y_skater_01"`) |
| `GetPlayerHealth(id)`, `SetPlayerHealth(id, health)` | GTA health: 200 full, 100 or less dead |
| `GetPlayerArmour(id)`, `SetPlayerArmour(id, armour)` | 0-100 |
| `IsPlayerDead(id)` | |
| `GivePlayerWeapon(id, weapon, ammo)` | weapon as hash or name (`"WEAPON_PISTOL"`) |
| `GivePlayerAmmo(id, weapon, ammo)` | |
| `GetPlayerWeapon(id)` | hash of the weapon in hand |
| `IsPlayerInVehicle(id)` | |
| `GetPlayerVehicle(id)` | vehicle id, or `nil` on foot |
| `GetPlayerSeat(id)` | `-1` driver, `0`.. passengers, `-2` on foot |
| `SetPlayerIntoVehicle(id, vehicle, seat)` | |
| `GetPlayerMoney(id)`, `SetPlayerMoney(id, amount)`, `GivePlayerMoney(id, amount)` | |
| `GetPlayerColor(id)`, `SetPlayerColor(id, rgba)` | colour of the name tag and blip, `0xRRGGBBAA` |
| `GetPlayerPing(id)` | milliseconds, `-1` when unknown |
| `GetPlayerAddress(id)` | `"ip|port"` |
| `GetPlayerClientVersion(id)` | the client's version string (`"unknown"` for the 2017 client) |
| `KickPlayer(id [, reason])` | shows the reason in the player's chat and disconnects them; `PlayerDisconnect` follows |

### Messages

| function | effect |
|---|---|
| `SendPlayerMessage(id, text [, rgba])` | a chat line for one player |
| `SendMessageToAll(text [, rgba])` | a chat line for everyone |
| `SendPlayerNotification(id, text)` | the game's notification above the minimap |
| `SetPlayerInfoMsg(id, text)` | a persistent line at the bottom of the screen; `SetPlayerInfoMsg(id, false)` removes it |

## Vehicles

| function | returns / effect |
|---|---|
| `CreateVehicle(model, x, y, z, heading)` | vehicle id; model as hash or name (`"adder"`) |
| `DeleteVehicle(id)` | `true` when it existed |
| `VehicleExists(id)` | |
| `GetVehicles()` | array of ids |
| `GetVehicleCoords(id)` | `x, y, z` (as last reported by its driver) |
| `SetVehicleCoords(id, x, y, z)` | moves it; the game client applies this once it handles `SetVehiclePos` (client work in progress) |
| `GetVehicleRotation(id)` | `x, y, z` |
| `GetVehicleHealth(id)` | |
| `GetVehicleModel(id)` | hash |
| `GetVehicleDriver(id)` | player id, or `nil` |

## Blips, markers, objects, 3D text

| function | returns / effect |
|---|---|
| `CreateBlipForAll(x, y, z, scale, color, sprite)` | blip id |
| `CreateBlipForPlayer(id, x, y, z, scale, color, sprite)` | blip id, visible to one player |
| `SetBlipColor(blip, color)`, `SetBlipRoute(blip, on)`, `DeleteBlip(blip)` | |
| `CreateMarkerForAll(x, y, z, height, radius)` | marker id; `EnterMarker` and `LeftMarker` fire for it |
| `CreateMarkerForPlayer(id, x, y, z, height, radius)` | marker id |
| `DeleteMarker(marker)` | |
| `CreateObject(model, x, y, z, pitch, yaw, roll)` | object id |
| `Create3DText(text, x, y, z, color, outlineColor, fontSize)` | text id |
| `Set3DTextText(text, content)` | |
| `Attach3DTextToVeh(text, vehicle, ox, oy, oz)`, `Attach3DTextToPlayer(text, player, ox, oy, oz)` | offsets in metres |
| `Delete3DText(text)` | |

## Client scripts, SQL, HTTP

* `AddClientScript("resources/<name>/client.lua")` compiles a Lua file and
  sends it to every client; the client side of the API is what the game
  client exposes (`orange-core/Scripting`).
* `SQLEnv()` returns a LuaSQL MySQL environment
  (`env:connect(db, user, password, host, port)`); it needs a MySQL client
  library on the machine (`libmariadb3` on Debian), nothing else does.
* The HTTP port answers `GET /` with the server status; `OnHTTPReq` sees
  every other request.

## A small game mode

```lua
local spawn = { 21.24, -711.04, 45.97 }
local admins = { ["127.0.0.1"] = true }

local function isAdmin(id)
	local ip = GetPlayerAddress(id):match("^[^|]+")
	return admins[ip] == true
end

OnEvent(function(name, ...)
	local a = { ... }
	if name == "PlayerConnect" then
		local id = a[1]
		SetPlayerCoords(id, spawn[1], spawn[2], spawn[3])
		GivePlayerWeapon(id, "WEAPON_PISTOL", 100)
		SendMessageToAll(GetPlayerName(id) .. " joined (" .. GetPlayerCount() .. "/" .. GetMaxPlayers() .. ")", RGBA(200, 200, 200))
	elseif name == "PlayerDisconnect" then
		SendMessageToAll(GetPlayerName(a[1]) .. " left")
	elseif name == "PlayerDeath" then
		local id = a[1]
		SetTimer(4000, function()
			if PlayerExists(id) then
				SetPlayerHealth(id, 200)
				SetPlayerCoords(id, spawn[1], spawn[2], spawn[3])
			end
		end)
	end
end)

OnCommand(function(id, text)
	local words = {}
	for w in text:gmatch("%S+") do words[#words + 1] = w end
	if words[1] == "/players" then
		for _, p in ipairs(GetPlayers()) do
			SendPlayerMessage(id, string.format("%d %s ping %d", p, GetPlayerName(p), GetPlayerPing(p)))
		end
		return false
	elseif words[1] == "/kick" and isAdmin(id) and words[2] then
		local target = tonumber(words[2])
		if target and PlayerExists(target) then KickPlayer(target, words[3] or "kicked by an admin") end
		return false
	end
	return true
end)

-- afk check: nobody has moved for ten minutes
SetInterval(60000, function()
	for _, p in ipairs(GetPlayers()) do
		if GetPlayerPing(p) > 800 then SendPlayerMessage(p, "Your connection is slow (" .. GetPlayerPing(p) .. " ms)") end
	end
end)
```

## Writing a module in C++

Modules are shared libraries in `modules/`; they receive the `API`
interface from `shared/ModuleAPI.h` (a virtual table: new functions are
appended at the end, never inserted, so older modules keep working) and
export `Validate`, `OnTick`, `OnEvent` and the other entry points
`Server/Plugin.cpp` looks up. `simple-module/` is the minimal example,
`lua-module/` the full one.
