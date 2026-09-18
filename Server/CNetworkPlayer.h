#pragma once
// One connected player, as the server knows it: what the client last
// reported (position, health, weapon, vehicle, ...) plus the server-side
// facts (id, name, money, colour). Players are created only by Create(), on
// ID_CONNECT_TO_SERVER; a packet from an unknown GUID is not a player.
class CNetworkPlayer
{
	static std::vector<CNetworkPlayer*> _players;                 // index = player id, nullptr = free slot
	static std::unordered_map<uint64_t, CNetworkPlayer*> _byGuid; // RakNetGUID.g -> player
	static void AddPlayer(CNetworkPlayer*);

	unsigned int uiID = 0;
	RakNet::RakNetGUID rnGUID;
	RakNet::SystemAddress rnAddress;
	std::string sClientVersion;
	unsigned int uProtocol = 1;              // ORANGE_PROTOCOL_VERSION the client announced (1: the 2017 client)
	static unsigned int _legacyPlayers;      // how many of them speak protocol 1
	Hash hModel = 0;
	bool bDead = false;
	bool bDucking = false;
	bool bBlipVisible = true;
	bool bHasPosition = false;     // at least one on-foot packet arrived
	float fHeading = 0.f;
	float fMoveSpeed = 0.f;
	unsigned long ulWeapon = 0;
	unsigned int uAmmo = 0;
	unsigned short usHealth = 200;
	unsigned short usArmour = 0;
	bool bJumping = false;
	bool bAiming = false;
	bool bShooting = false;
	bool bEnteringVeh = false;
	CVector3 vecPosition;
	CVector3 vecRotation;
	CVector3 vecMoveSpeed;
	CVector3 vecAim;
	OnFootSyncData lastSync;       // the last packet as received; what the snapshots carry
	std::string sName;
	size_t uMoney = 0;
	float fTagDrawDistance = 50.f;
	unsigned long ulLastVehUpdate = 0;
	unsigned long ulLastSyncMs = 0;      // when the client last sent on-foot data (server clock, ms)
	unsigned long ulSyncPackets = 0;     // accepted on-foot packets
	unsigned long ulSyncDropped = 0;     // dropped by the per-client rate cap
	color_t colColor;
public:
	CNetworkPlayer(RakNet::RakNetGUID GUID);   // registers the player; prefer Create()
	~CNetworkPlayer();

	bool bInVehicle = false;
	RakNetGUID vehicle;
	char cSeat = -2;

	static CNetworkPlayer * Create(RakNet::RakNetGUID GUID, const RakNet::SystemAddress & address);
	static void Each(void(*func)(CNetworkPlayer*));
	// nullptr when no player has this GUID (never creates one).
	static CNetworkPlayer *GetByGUID(RakNet::RakNetGUID GUID);
	static CNetworkPlayer *GetByID(UINT playerID);
	static void Tick();
	static UINT Count();
	// The slot table: may hold nullptr entries.
	static const std::vector<CNetworkPlayer *> & All();
	static void Remove(int playerid);

	unsigned int GetID() { return uiID; }
	RakNet::RakNetGUID GetGUID() { return rnGUID; }
	const RakNet::SystemAddress & GetAddress() { return rnAddress; }
	void SetClientVersion(const std::string & version) { sClientVersion = version; }
	const std::string & GetClientVersion() { return sClientVersion; }
	void SetProtocol(unsigned int protocol);
	unsigned int GetProtocol() const { return uProtocol; }
	// true for a client that expects the 2017 relay (ID_SEND_PLAYER_DATA with
	// the sender's name) instead of snapshots
	bool IsLegacy() const { return uProtocol < 2; }
	static unsigned int LegacyCount() { return _legacyPlayers; }
	void SetOnFootData(const OnFootSyncData& data);
	void GetOnFootData(OnFootSyncData & data);
	const OnFootSyncData & LastSync() { return lastSync; }
	bool HasPosition() { return bHasPosition; }
	unsigned long LastSyncMs() { return ulLastSyncMs; }
	// The per-client rate cap: true when this packet is accepted.
	bool AcceptSync(unsigned long nowMs, unsigned int maxRateHz);
	unsigned long SyncPackets() { return ulSyncPackets; }
	unsigned long SyncDropped() { return ulSyncDropped; }
	void HideBlip() { bBlipVisible = false; }
	void ShowBlip() { bBlipVisible = true; }
	bool IsBlipVisible() { return bBlipVisible; }
	void SetName(std::string playername) { sName = playername; }
	std::string GetName() { return sName; }
	void SetMoney(size_t money) { uMoney = money; }
	void GiveMoney(size_t money) { uMoney += money; }
	size_t GetMoney() { return uMoney; }
	void SetPosition(const CVector3& position);
	void SetCoords(const CVector3 & position);
	void GetPosition(CVector3& position) { position = vecPosition; };
	CVector3 GetPosition() { return vecPosition; };
	void SetHeading(float heading);
	void GiveWeapon(unsigned int weaponHash, unsigned int ammo);
	void GiveAmmo(unsigned int weaponHash, unsigned int ammo);
	void SetModel(unsigned int model);
	void SetHealth(float health);
	void SetArmour(float armour);
	void SetColor(unsigned int color);
	color_t GetColor() { return colColor; }
	float GetHealth() { return (float)usHealth; }
	float GetArmour() { return (float)usArmour; }
	unsigned int GetModel() { return hModel; }
	float GetHeading() { return fHeading; }
	unsigned long GetWeapon() { return ulWeapon; }
	bool IsDead() { return bDead; }
	float GetTagDrawDistance() { return fTagDrawDistance; }
	void SendTextMessage(const char * message, unsigned int color);
};
