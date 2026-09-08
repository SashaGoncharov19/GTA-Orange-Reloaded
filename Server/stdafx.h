#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <cmath>
#include <cstring>
#include <string>
#include <locale>
#include <vector>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <time.h>
#include <thread>
#include <iomanip>

#ifdef _WIN32

// Windows Header Files:
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <Psapi.h>
#include <direct.h>
#include <TimeAPI.h>

#else

#include <unistd.h>

// Win32 style type names used by the shared network structures. They must
// keep the exact same size as on Windows because they travel over the wire.
typedef uint32_t DWORD;
typedef unsigned char BYTE;
typedef unsigned int UINT;
typedef unsigned long ULONG;
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#endif

// YAML
#include "yaml-cpp/yaml.h"

// Config
#include "CConfig.h"

// Logging
#include <Console/CConsole.h>
#include <CLog.h>

// RakNet
#include <MessageIdentifiers.h>
#include <RakPeerInterface.h>
#include <RakNetStatistics.h>
#include <RakNetTypes.h>
#include <BitStream.h>
#include <RakSleep.h>
#include <PacketLogger.h>
#include <Gets.h>
#include <WindowsIncludes.h>
#include <GetTime.h>
#include <RPC4Plugin.h>
using namespace RakNet;

// Scripthook types
#include <types.h>

// Network manager
#include "CNetworkConnection.h"

// Shared stuff from MTA
#include "CMath.h"
#include "CVector3.h"
#include "NetworkTypes.h"

// RPC
#include "CRPCPlugin.h"

// API
#include "API.h"
#include "Plugin.h"
#include "CClientScripting.h"

// Network objects
#include "CNetworkPlayer.h"
#include "CNetworkVehicle.h"
#include "CNetworkBlip.h"
#include "CNetworkMarker.h"
#include "CNetwork3DText.h"
#include "CNetworkObject.h"

//Http Server
#include "CivetServer.h"
#include "CHTTPServer.h"

unsigned long createGUID();
