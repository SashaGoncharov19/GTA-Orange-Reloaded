#pragma once
class CNetworkVehicle
{
	static std::vector<CNetworkVehicle *> Vehicles;

public:
	RakNet::RakNetGUID rnGUID;
	CNetworkPlayer* driver;				// vehicle driver
	Hash hashModel;						// vehicle model
	CVector3 vecPos;					// vehicle position
	CVector3 vecRot;					// vehicle rotation
	CVector3 vecMoveSpeed;				// vehicle move speed
	unsigned short int usHealth;		// vehicle health 
	bool bTaxiLights;					// vehicle taxi lights
	bool bSirenState;					// vehicle siren state
	float iDirtLevel;					// vehicle dirt level

	float fEngineHealth = 1000, fBodyHealth = 1000, fTankHealth = 1000;
	bool bDrivable = true;

	RakNetGUID driverGUID;
	bool hasDriver = false;
	unsigned long ulLastUpdateMs = 0;   // server clock, when a client last sent this vehicle's state

	// Whether a packet about this vehicle from `from` is the driver's (or the
	// vehicle is free, or the previous driver went quiet for two seconds).
	bool AcceptsStateFrom(RakNetGUID from, unsigned long nowMs);

	static std::vector<CNetworkVehicle *> All();
	static void SendGlobal(RakNet::Packet * packet);
	static int Count();
	static CNetworkVehicle *GetByGUID(RakNetGUID);

	CNetworkVehicle(Hash model, float x, float y, float z, float heading);

	RakNetGUID GetGUID();

	void SetPosition(CVector3 position);
	void SetDriver(CNetworkPlayer* driver);
	void SetRotation(CVector3 rotation);
	void SetHealth(unsigned short health);

	CVector3 GetPosition() { return vecPos; };

	void SetVehicleData(const VehicleData& data);
	void GetVehicleData(VehicleData& data);

	~CNetworkVehicle();
};

