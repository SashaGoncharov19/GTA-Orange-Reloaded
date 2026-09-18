#pragma once
class SResource
{
public:
	SResource();
	bool Init();
	void AddClientScript(std::string file);
	bool Start(const char * name);
	static SResource *singleInstance;
	static SResource *Get();
	bool OnTick();
	// Timers (SetTimer / SetInterval / ClearTimer): callbacks kept as
	// registry references, fired from OnTick.
	int AddTimer(int ref, unsigned long intervalMs, bool repeat);
	bool RemoveTimer(int id);
	lua_State * State() { return m_lua; }
	bool OnPlayerCommand(long playerid, const char * cmd);
	void SetHTTP(const std::function<char*(const char* method, const char* url, const char* query, const char* body)>& t);
	void SetTick(const std::function<void()>& t);
	void SetEvent(const std::function<void(const char*e, std::vector<MValue> *args)>& t);
	void SetCommandProcessor(const std::function<bool(long pid, const char* command)>& t);
	char * OnHTTPRequest(const char * method, const char * url, const char * query, const char * body);
	bool OnKeyStateChanged(long playerid, int keycode, bool isUp);
	void OnEvent(const char * e, std::vector<MValue> *args);
	~SResource();
private:
	struct Timer
	{
		int id;
		int ref;
		unsigned long dueMs;
		unsigned long intervalMs;
		bool repeat;
		bool removed;
	};
	std::vector<Timer> m_timers;
	int m_nextTimerId = 1;
	void RunTimers();
	lua_State *m_lua;
	std::function<void()> tick;
	std::function<char*(const char* method, const char* url, const char* query, const char* body)> http;
	std::function<void(const char* e, std::vector<MValue> *args)> onevent;
	std::function<bool(long pid, const char* command)> oncommand;
};

