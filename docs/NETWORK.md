# Network protocol and synchronisation

How the server and the clients talk, what is sent how often, and what the
numbers look like at 1000 players. Read this before touching
`Server/CNetworkConnection.cpp`, `orange-core/Network/*` or
`shared/NetworkTypes.h`.

## Transport

* RakNet over UDP, one `RakPeer` on the server (port 7788 by default) and one
  per client. The HTTP server on port 7789 is separate and only answers
  status queries.
* Two ordering channels (`shared/NetworkTypes.h`):

  | channel | constant | used for |
  |---|---|---|
  | 0 | `ORANGE_CHANNEL_RELIABLE` | everything that must arrive: handshake, entities, RPCs, chat, tasks |
  | 1 | `ORANGE_CHANNEL_STATE` | player and vehicle state: lost packets are replaced by the next one |

* The protocol number is `ORANGE_PROTOCOL_VERSION` (2). A client sends it in
  the handshake; the server logs it and does not refuse older clients yet.
* Structures travel as raw bytes (`bitStream.Write(struct)`), so their layout
  is part of the protocol: `OnFootSyncData` is 96 bytes and `VehicleData` is
  120 bytes on MSVC and GCC alike, guarded by `static_assert`s in
  `NetworkTypes.h`. Change a field and both sides need a new build.

## Joining

1. RakNet connection. The server receives `ID_NEW_INCOMING_CONNECTION` and
   sends the newcomer everything global: the model list to preload (RPC
   `PreloadModels`), blips, markers, vehicles, 3D texts, objects and the
   client scripts of the running resources.
2. The client sends `ID_CONNECT_TO_SERVER`:
   `RakString name`, `RakString clientVersion`, `uint32 protocol`
   (the 2017 client and `orange_handshake` sent the name only; both extras
   are optional).
3. The server creates the player (small integer id, the RakNet GUID as the
   key), fires `PlayerConnect` for the resources, and answers with an empty
   `ID_CONNECT_TO_SERVER`. It then sends the newcomer one `ID_PLAYER_INFO`
   per player already online and broadcasts the newcomer's own record:
   `RakNetGUID guid, uint32 id, RakString name, uint32 model, color_t color`.
4. From here the client sends its state and receives snapshots.

A client that announces protocol 1, or no protocol at all (the 2017
client), is served the 2017 way instead: no `ID_PLAYER_INFO`, and every
accepted state packet of the players within `stream_distance` is relayed to
it as `ID_SEND_PLAYER_DATA` with `RakNetGUID, RakString name, OnFootSyncData`
(reliable ordered, as before). Current clients never receive that relay.

Anything that arrives before step 2 from a connection (chat, state, tasks)
is dropped; a packet with an unknown identifier is logged once per
identifier and then ignored. Names are cut to 31 printable characters,
chat and commands to 256. A full server closes the connection.

Leaving: `ID_DISCONNECTION_NOTIFICATION` (reason 1) or `ID_CONNECTION_LOST`
(reason 2) fires `PlayerDisconnect(id, reason)` while the player still
exists, then removes it and broadcasts `ID_PLAYER_LEFT guid`.

## Player state

A client sends `ID_SEND_PLAYER_DATA` + `OnFootSyncData` on channel 1,
`UNRELIABLE_SEQUENCED`, 20 times a second on foot and 30 in a vehicle
(`ORANGE_CLIENT_SYNC_RATE_*`). The server keeps the latest state per player
and drops what arrives faster than `max_client_sync_rate` allows (it accepts
at most one packet per half of the minimum interval, so a burst after a lag
spike still gets through).

The server does not forward these packets. Every `1000 / sync_rate` ms it
builds one batch per player, the states of the players around it, and sends
it as `ID_PLAYER_SNAPSHOT` datagrams:

```
uint8  ID_PLAYER_SNAPSHOT
uint32 serverTimeMs      time the batch was built (RakNet::GetTimeMS)
uint8  count             entries in this datagram
count x { RakNetGUID guid; OnFootSyncData state; }     104 bytes each
```

* **Distance tiers.** With `stream_distance` R: a player within 0.2R is in
  every batch, within 0.6R in every second one, within R in every fourth,
  beyond R never. A player whose last state is older than 5 s is left out
  (the receiver removes it after its own timeout).
* **Cap.** At most `max_streamed_players` entries per batch; when more
  players qualify, the nearest win.
* **Datagram size.** A batch is cut into datagrams that fit the receiver's
  MTU (13 entries at 1492, 10 at 1200, 4 at 576). This matters: RakNet
  silently turns any unreliable message larger than one datagram into a
  reliable one, with retransmissions and head-of-line blocking, which is the
  opposite of what state needs.
* **Reliability.** `UNRELIABLE` on channel 1. Datagrams can arrive out of
  order, so the receiver keeps the newest `serverTimeMs` per remote player
  and ignores an entry whose datagram is older than what it already applied.
* A player who has not reported a position yet receives nothing (there is
  nowhere to stream around); `stream_distance: 0` disables the tiers and the
  cap picks the first players in the list.
* Neighbours are looked up through a grid of R-sized cells, rebuilt every
  period from a contiguous array of positions, so the cost grows with the
  number of players near each other, not with the square of everyone online.

Vehicle state (`ID_SEND_VEHICLE_DATA` + `VehicleData`) is relayed as it
arrives, `UNRELIABLE_SEQUENCED` on channel 1, to the players within
`stream_distance` of the vehicle. Only the driver's packets are accepted; a
vehicle nobody has updated for 2 s can be claimed by whoever sends next.

Tasks (`ID_SEND_TASKS`) stay reliable on channel 0 and are relayed to the
players near the sender.

## What the client does with it

`orange-core/Network/CNetworkConnection.cpp`:

* `ID_PLAYER_INFO` records go into a who-is-who list (`RemotePlayerInfo`:
  id, name, model, colour) that outlives the ped. A player's ped exists
  only while the server streams that player to us.
* The first `ID_PLAYER_SNAPSHOT` entry for a GUID creates the ped with the
  entry's model at the entry's position (the info record, when it arrived
  first, supplies the name and colour); later entries feed the
  interpolation, whose delay is the measured interval between updates for
  that player, clamped to 50-200 ms. An entry from a datagram older than
  the last one applied is ignored (`AcceptServerTime`).
* A remote player without state for 10 s is deleted
  (`CNetworkPlayer::Tick`), which is how streaming out and lost connections
  look from here; `ID_PLAYER_LEFT` deletes it at once and forgets the
  record.
* The 2017 relay (`ID_SEND_PLAYER_DATA` with the name) is still understood,
  for a server that has not been updated.
* Sending: `CLocalPlayer::SendOnFootData` runs every frame but sends only
  every 50 ms on foot and every 33 ms while driving, `UNRELIABLE_SEQUENCED`
  on channel 1; the vehicle state goes with it when we are the driver of a
  server vehicle.
* A lost or closed connection clears the remote players and shows the
  server browser again.

## Server-side messages and RPCs

The rest goes through the RakNet RPC4 plugin, reliable on channel 0, by name:
`SendClientMessage`, `SetPlayerHeading`, `SetVehiclePos`, `PreloadModels`,
the blip/marker/object/3D text create-and-update calls, `CreateVehicle`,
`SetPlayerIntoVehicle`, weapons, health, and so on (`Server/API.cpp` is the
list). A client script raises `serverEvent` through the `ServerEvent` RPC
(`RakString name, int32 count, count x { int32 type; value }` with type 0 bool,
1 double, 2 string).

## Configuration (`config.yml`)

| key | default | meaning |
|---|---|---|
| `players` | 128 | connection slots, at most 4096 |
| `stream_distance` | 500 | metres; 0 streams everyone |
| `sync_rate` | 20 | snapshots per second per player, 1-60 |
| `max_streamed_players` | 32 | entries per snapshot, 1-255 |
| `max_client_sync_rate` | 40 | state packets per second accepted from one client |

Bandwidth to one client is roughly
`entries x 104 bytes x sync_rate`: 32 entries at 20 Hz are 66 KB/s, a
typical busy spot (12 entries after the tiers) 25 KB/s. Inbound is
`20 x 97 bytes` = 2 KB/s per client.

## The statistics line

Every 30 s the server logs one line:

```
Stats: 1000 player(s); loop 172/s, 10719 packet(s)/s in (1.0% of a core);
24023 snapshot datagram(s)/s with 10 player(s) each (10.6% of a core);
1256 KB/s in, 26738 KB/s out (26 KB/s per client), packet loss 0.00%
```

* `loop` is the network loop rate; it idles at 200/s (`RakSleep(5)`). When it
  falls well below `sync_rate` the server no longer keeps up.
* the two percentages are the main thread's time in packet handling and in
  building snapshots; RakNet's own thread (sending, acks) is not included.
* bytes and packet loss are summed over all connections.

## Load test

`orange_bot` (built next to the server) is N players without a game: every
bot is its own RakNet peer, joins like the client (name, version, protocol),
sends `OnFootSyncData` at the client's rate while walking a small circle,
and counts what comes back.

```
./orange_bot --bots 20 --duration 5                    # the CI smoke test
./orange_bot --bots 1000 --duration 60 --spread 2000 --threads 4 --quiet
```

`--spread` is the radius of the disc the bots are placed in; larger than
`stream_distance` means every bot only sees its neighbours. Exit code 0 when
every bot was accepted and, with more than one bot, every bot received at
least one player state.

Measured on 2026-09-18 with 1000 bots in a 2 km disc, `stream_distance`
500, `sync_rate` 20, `max_streamed_players` 32, server pinned to two cores
of a 4-core VM (the bots need two and a half cores themselves):

| | value |
|---|---|
| players | 1000, all accepted, none dropped |
| network loop | 172/s (target 200) |
| snapshot datagrams | 24 000/s, 10 states each, 10.6% of a core |
| packets in | 10 700/s, 1.0% of a core |
| traffic | 1.3 MB/s in, 26.7 MB/s out, 26 KB/s per client |
| packet loss | 0.00% |

Before the grid and the datagram split the same test delivered 9.6
snapshots per second per player instead of 20, with 40% of a core in the
pair loop.

## What comes next

* **Compact entries.** 104 bytes per player state is the lever for bandwidth
  at 1000 players (quantised heading, velocity and aim, no model hash in
  every packet); both sides change together.
* **Vehicles** through the same batch path as players, and a vehicle
  stream-out on the client.
* **Priority** inside the cap: players in view or aiming at each other
  before those behind.
* **Movement quality** on the client: the remote ped is still driven by a
  go-to task re-issued every frame plus velocity; a proper motion blend
  (walk/run/sprint by reported speed, strafing while aiming) is the next
  gameplay step.
