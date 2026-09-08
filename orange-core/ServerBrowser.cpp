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

// The game keeps hiding the Windows cursor, so a single ShowCursor(TRUE) is
// undone within a frame (the first run with that change had no visible cursor
// at all): ask for it every frame while the window is shown, as 2017 did, and
// let ImGui draw its own cursor on top, which does not depend on the game's
// cursor handling at all. When the window goes, the display counter is driven
// below zero once, so the cursor does not stay on the screen while playing.
static void CursorForBrowser(bool shown)
{
	static bool cursorShown = false;
	ImGui::GetIO().MouseDrawCursor = shown;
	if (shown)
	{
		ShowCursor(TRUE);
		if (!cursorShown)
			(*CGlobals::Get().canLangChange) = true;
		cursorShown = true;
		return;
	}
	if (cursorShown)
	{
		while (ShowCursor(FALSE) >= 0) {}
		(*CGlobals::Get().canLangChange) = false;
		cursorShown = false;
	}
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
