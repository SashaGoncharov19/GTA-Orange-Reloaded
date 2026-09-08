#include "stdafx.h"

CConfig *CConfig::singleInstance = nullptr;

static void CopyToGlobals(const CConfig & config)
{
	strncpy_s(CGlobals::Get().serverIP, sizeof(CGlobals::Get().serverIP), config.sIP.c_str(), _TRUNCATE);
	CGlobals::Get().serverPort = (int)config.uiPort;
	strncpy_s(CGlobals::Get().nickName, sizeof(CGlobals::Get().nickName), config.sNickName.c_str(), _TRUNCATE);
}

CConfig::CConfig()
{
	std::string path = CGlobals::Get().orangePath + "/config.xml";
	try {
		doc.LoadFile(path.c_str());
		tinyxml2::XMLElement * root = doc.Error() ? nullptr : doc.FirstChildElement("config");
		if (!root)
		{
			log_info << "Config: " << path << " is missing or unreadable, writing the defaults" << std::endl;
			InitConfig();
			return;
		}
		tinyxml2::XMLElement * serverNode = root->FirstChildElement("server");
		if (serverNode && serverNode->GetText() && *serverNode->GetText())
		{
			sIP = serverNode->GetText();
			int port = serverNode->IntAttribute("port", 7788);
			uiPort = (port > 0 && port <= 65535) ? (unsigned int)port : 7788;
		}
		tinyxml2::XMLElement * playerNode = root->FirstChildElement("player");
		if (playerNode && playerNode->GetText() && *playerNode->GetText())
			sNickName = playerNode->GetText();
		CopyToGlobals(*this);
		log_info << "Config: server " << sIP << ":" << uiPort << ", nickname '" << sNickName << "' (" << path << ")" << std::endl;
	}
	catch (...)
	{
		InitConfig();
	}
}

void CConfig::InitConfig()
{
	sNickName = "Player";
	sIP = "127.0.0.1";
	uiPort = 7788;
	CopyToGlobals(*this);
	Save();
}

bool CConfig::Save()
{
	tinyxml2::XMLDocument out;
	tinyxml2::XMLNode * pRoot = out.NewElement("config");
	out.InsertFirstChild(pRoot);

	tinyxml2::XMLElement * pServer = out.NewElement("server");
	pServer->SetText(sIP.c_str());
	pServer->SetAttribute("port", (int)uiPort);
	pRoot->InsertEndChild(pServer);

	tinyxml2::XMLElement * pPlayer = out.NewElement("player");
	pPlayer->SetText(sNickName.c_str());
	pRoot->InsertEndChild(pPlayer);

	std::string path = CGlobals::Get().orangePath + "/config.xml";
	tinyxml2::XMLError eResult = out.SaveFile(path.c_str());
	if (eResult != tinyxml2::XMLError::XML_SUCCESS)
	{
		log_error << "Config: could not write " << path << " (tinyxml2 error " << (int)eResult << ")" << std::endl;
		return false;
	}
	return true;
}

CConfig::~CConfig()
{
}
