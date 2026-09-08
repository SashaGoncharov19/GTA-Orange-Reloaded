-- GTA:Orange Lua API bootstrap.
--
-- lua-module loads this file right after creating the Lua state and before any
-- resource is started. The native module registers all of its functions in the
-- __orange__ table; this file exposes them to resources as plain globals and
-- as the `orange` table, e.g.:
--
--   orange.OnEvent(function(name, ...) ... end)
--   CreateVehicle("adder", 0.0, 0.0, 72.0, 90.0)

orange = __orange__

for name, fn in pairs(__orange__) do
	if _G[name] == nil then
		_G[name] = fn
	end
end

-- Called by lua-module for key events (optional to override in a resource).
function __OnKeyStateChanged(playerid, keycode, isUp)
end

-- Convenience: RGBA -> the 0xRRGGBBAA colour format used by the API.
function RGBA(r, g, b, a)
	a = a or 255
	return ((r % 256) * 0x1000000) + ((g % 256) * 0x10000) + ((b % 256) * 0x100) + (a % 256)
end
