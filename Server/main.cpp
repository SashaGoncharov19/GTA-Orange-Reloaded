#include "stdafx.h"

#include <atomic>
#include <chrono>
#include <csignal>

#ifndef _WIN32
#include <sys/time.h>
#endif

int counter = 1;
unsigned long createGUID() { return counter++; }

#ifndef _WIN32
unsigned long GetTickCount()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}
#endif

static std::atomic<bool> g_running(true);

static void OnSignal(int)
{
	g_running = false;
}

int main(void)
{
	std::signal(SIGINT, OnSignal);
	std::signal(SIGTERM, OnSignal);

#ifndef ORANGE_VERSION
#define ORANGE_VERSION "dev"
#endif
	log << "GTA:Orange server " << ORANGE_VERSION << std::endl;
	log << "Starting the server..." << std::endl;
	log << "Hostname: " << /*color::lred <<*/ CConfig::Get()->Hostname << std::endl;
	log << "Port: " << /*color::lred <<*/ CConfig::Get()->Port << std::endl;
	log << "HTTP Server port: " << /*color::lred <<*/ CConfig::Get()->HTTPPort << std::endl;
	log << "Maximum players: " << /*color::lred <<*/ CConfig::Get()->MaxPlayers << std::endl;

	Plugin::LoadPlugins();

	CHTTPServer::Get()->Start(CConfig::Get()->HTTPPort);
	CHTTPHandler h_a;
	CHTTPServer::g_server->addHandler("", h_a);

	auto netLoop = [=]()
	{
		CNetworkConnection::Get()->Start(CConfig::Get()->MaxPlayers, CConfig::Get()->Port);
		CRPCPlugin::Get();
		DWORD lastTick = 0;
		RakNet::RakNetStatistics stat;

		while (g_running)
		{
			RakSleep(5);
			CNetworkConnection::Get()->Tick();
			CNetworkPlayer::Tick();
			CNetworkMarker::Tick();
			Plugin::Tick();

			if ((GetTickCount() - lastTick) > 100)
			{
				CNetworkConnection::Get()->server->GetStatistics(0, &stat);
				std::stringstream ss;
				ss << CConfig::Get()->Hostname << ". Players online: " << CNetworkPlayer::Count() << ", "
					<< "Packet loss: " << std::setprecision(2) << std::fixed << stat.packetlossTotal * 100 << "%";
				//SetConsoleTitle(ss.str().c_str());
				lastTick = GetTickCount();
			}
		}
	};
	std::thread netThread(netLoop);

	// Console loop. When stdin is not a terminal (systemd, Docker, nohup) the
	// server simply keeps running until it receives SIGINT/SIGTERM.
	std::string msg;
	while (g_running)
	{
		if (!std::getline(std::cin, msg))
		{
			while (g_running)
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			break;
		}
		if (!msg.compare("exit") || !msg.compare("kill"))
		{
			g_running = false;
			break;
		}
		else if (!msg.empty())
		{
			Plugin::ServerCommand(msg);
		}
	}

	log << "Terminating server..." << std::endl;
	g_running = false;
	if (netThread.joinable())
		netThread.join();
	return 0;
}
