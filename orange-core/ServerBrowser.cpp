#include "stdafx.h"

// The browser is drawn on the render thread. Connecting deletes the remote
// peds through natives and natives belong to the script thread, so the
// button only queues the work (ScriptDispatcher runs it on the next tick).
static void QueueConnect()
{
	std::string host = CGlobals::Get().serverIP;
	unsigned short port = (unsigned short)CGlobals::Get().serverPort;

	CConfig::Get()->sNickName = std::string(CGlobals::Get().nickName);
	CConfig::Get()->sIP = host;
	CConfig::Get()->uiPort = port;
	CConfig::Get()->Save();

	CScriptInvoker::Get().Push([=]() { CNetworkConnection::Get()->ConnectTo(host, port); });

	CGlobals::Get().displayServerBrowser = false;
	CGlobals::Get().showChat = true;
}

// ShowCursor keeps a counter: one TRUE when the window appears, one FALSE
// when it goes (the 2017 code called ShowCursor(TRUE) every frame, and the
// cursor stayed on the screen after connecting).
static void CursorForBrowser(bool shown)
{
	static bool cursorShown = false;
	if (shown == cursorShown)
		return;
	cursorShown = shown;
	ShowCursor(shown ? TRUE : FALSE);
	(*CGlobals::Get().canLangChange) = shown;
}

void ServerBrowser()
{
	CursorForBrowser(CGlobals::Get().displayServerBrowser);
	if (!CGlobals::Get().displayServerBrowser)
		return;

	CConfig::Get();
	ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiSetCond_Always);
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_Always);
	ImGui::PushFont(CGlobals::Get().chatFont);
	ImGui::Begin("Server browser", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
	ImGui::Text("Nickname");
	ImGui::InputText("  ", CGlobals::Get().nickName, 32);
	ImGui::Text("Server");
	ImGui::InputText(":", CGlobals::Get().serverIP, 32);
	ImGui::SameLine();
	ImGui::InputInt("Port", &CGlobals::Get().serverPort, 1, 100);
	if (CGlobals::Get().serverPort < 1)
		CGlobals::Get().serverPort = 1;
	if (CGlobals::Get().serverPort > 65535)
		CGlobals::Get().serverPort = 65535;
	ImGui::Spacing();
	if (ImGui::Button("Connect"))
		QueueConnect();
	ImGui::Spacing();
	ImGui::TextDisabled("The address is kept in config.xml.");
	ImGui::TextDisabled("Chat (T): /connect host:port, /disconnect. F12 shows this window again.");
	ImGui::End();
	ImGui::PopFont();
}

GUI(ServerBrowser);
