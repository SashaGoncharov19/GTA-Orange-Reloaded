#pragma once
// config.xml next to orange-core.dll:
//   <config>
//     <server port="7788">127.0.0.1</server>
//     <player>Nickname</player>
//   </config>
class CConfig
{
	static CConfig *singleInstance;
	CConfig();
	void InitConfig();
public:
	static CConfig *Get()
	{
		if (!singleInstance)
			singleInstance = new CConfig();
		return singleInstance;
	}

	tinyxml2::XMLDocument doc;
	std::string sNickName = "Player";
	std::string sIP = "127.0.0.1";
	unsigned int uiPort = 7788;

	// Writes config.xml; a failure is logged, never thrown (callers run on
	// the render thread).
	bool Save();
	~CConfig();
};
