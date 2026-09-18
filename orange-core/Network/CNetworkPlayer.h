#pragma once

// What the server said about a player (ID_PLAYER_INFO). Kept for as long as
// the player is online, whether or not its ped is streamed in right now.
struct RemotePlayerInfo
{
	unsigned int id = 0;
	std::string name;
	Hash model = 0;
	color_t color = { 0xFF, 0x8F, 0x00, 0xFF };
};

struct tag_t {
	bool bVisible;
	float health, distance;
	float x, y;
	float width, height;
	float k;
};

class CNetworkPlayer: public CPedestrian
{
private:
	static std::vector<CNetworkPlayer *> PlayersPool;
	static std::unordered_map<uint64_t, RemotePlayerInfo> Known;
	struct
	{
		struct
		{
			CVector3      vecStart;
			CVector3      vecTarget;
			CVector3      vecError;
			float         fLastAlpha;
			unsigned long ulStartTime;
			unsigned long ulFinishTime;
		} pos;
		struct
		{
			float         fStart;
			float         fTarget;
			float         fError;
			float         fLastAlpha;
			unsigned long ulStartTime;
			unsigned long ulFinishTime;
		} heading;
		struct
		{
			CVector3      vecStart;
			CVector3      vecTarget;
			CVector3      vecError;
			float         fLastAlpha;
			unsigned long ulStartTime;
			unsigned long ulFinishTime;
		} rot;
	}					m_interp;
	RakNet::RakNetGUID	m_GUID;
	std::string			m_Name;
	CVector3			m_vecMove;
	CVector3			m_vecAim;
	Hash				m_Model;
	bool				m_Spawned = false;
	bool				m_Ducking = false;
	bool				m_Jumping = false;
	bool				m_JustJumping = false;
	bool				m_TagVisible = true;
	bool				m_Aiming = false;
	bool				m_Shooting = false;
	bool				m_InVehicle = false;
	RakNetGUID			m_Vehicle;
	bool				m_Entering = false;
	bool				m_Lefting = false;
	bool				m_Ragdoll = false;

	bool				pedJustDead = false;
	float				m_MoveSpeed;
	float				lastMoveSpeed;
	int					updateTick = 0;
	DWORD				lastTick = 0;
	int					tasksToIgnore = 0;
	DWORD				lastUpdate = 9999;
	DWORD				timeEnterVehicle = 0;
	DWORD				timeLeaveVehicle = 0;
	unsigned short		m_Health = 200;
	unsigned int		m_ServerTime = 0;     // server time of the last state applied
	bool				m_HasState = false;
	unsigned int		m_Id = 0;
	color_t				m_Color = { 0xFF, 0x8F, 0x00, 0xFF };
	tag_t				tag;
	std::queue<std::function<void()>> taskQueue;
	CNetworkPlayer(RakNet::RakNetGUID guid);
public:
	CPed* pedHandler = nullptr;
	short m_Seat;
	short m_FutureSeat;
	static int ignoreTasks;
	static Hash hFutureModel;            // model of the next ped created
	static CVector3 vecFuturePosition;   // and where it appears
	static std::vector<CNetworkPlayer*> All();
	static void DeleteByGUID(RakNet::RakNetGUID guid);
	// create: make the ped (hFutureModel at vecFuturePosition, or the known
	// model) when the player is not streamed in yet
	static CNetworkPlayer * GetByGUID(RakNet::RakNetGUID GUID, bool create = false);
	static bool Exists(RakNet::RakNetGUID GUID);
	static CNetworkPlayer * GetByHandler(Entity handler);
	static void Clear();
	static void Tick();

	// the who-is-who list from ID_PLAYER_INFO
	static void Remember(RakNet::RakNetGUID guid, const RemotePlayerInfo & info);
	static const RemotePlayerInfo * Info(RakNet::RakNetGUID guid);
	static void Forget(RakNet::RakNetGUID guid);
	static size_t KnownCount() { return Known.size(); }

	// false when a state with this server time is older than the one applied
	bool AcceptServerTime(unsigned int serverTime);
	void MarkHasState() { m_HasState = true; }
	bool HasState() { return m_HasState; }
	unsigned int GetId() { return m_Id; }
	void SetId(unsigned int id) { m_Id = id; }
	void SetColor(color_t color) { m_Color = color; }
	static void PreRender();
	static void Render();

	
	void UpdateLastTickTime();
	int GetTickTime();
	
	std::string GetName() { return m_Name; }
	void SetName(std::string Name) { m_Name = Name; }

	bool IsSpawned();
	void SetPosition(const CVector3 & vecPosition, bool bResetInterpolation);
	void SetRotation(const CVector3 & vecRotation, bool bResetInterpolation);
	void Spawn(const CVector3& vecPosition);

	void SetTargetPosition(const CVector3& vecPosition, unsigned long ulDelay);
	void SetTargetRotation(const CVector3& vecRotation, unsigned long ulDelay);
	void SetOnFootData(OnFootSyncData data, unsigned long ulDelay);

	bool HasTargetPosition() { return (m_interp.pos.ulFinishTime != 0); }
	bool HasTargetRotation() { return (m_interp.rot.ulFinishTime != 0); }

	void UpdateTargetPosition();
	void UpdateTargetRotation();

	void SetModel(Hash model);

	void RemoveTargetPosition();
	void RemoveTargetRotation();
	void ResetInterpolation();

	void SetMovementTask(RakNet::BitStream& bsIn);

	void Interpolate();

	void SetMoveToDirection(CVector3 vecPos, CVector3 vecMove, float iMoveSpeed);

	void SetMoveToDirectionAndAiming(CVector3 vecPos, CVector3 vecMove, CVector3 aimPos, float moveSpeed, bool shooting);

	void AssignTask(GTA::CTask * task);

	void BuildTasksQueue();

	void MakeTag();
	void DrawTag();

	~CNetworkPlayer()
	{
		PED::DELETE_PED(&Handle);
	}
};
