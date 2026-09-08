#pragma once
#ifndef _IOLOG_
#define _IOLOG_

#include <ctime>
#include <iostream>
#include <string>

#define _O_LOG_DEBUG "[Debug] "
#define _O_LOG_ERROR "[Error] "
#define _O_LOG_INFO "[Info] "

#if !defined PROJECT_NAME
#define PROJECT_NAME ""
#endif

// Portable "local time as struct tm" helper (localtime_s on Windows, localtime_r elsewhere).
inline bool OrangeLocalTime(tm& out)
{
	time_t rawtime;
	time(&rawtime);
#ifdef _WIN32
	return localtime_s(&out, &rawtime) == 0;
#else
	return localtime_r(&rawtime, &out) != nullptr;
#endif
}

class my_ostream
{
public:

	my_ostream() {
	};

	my_ostream& info()
	{
		std::cout << color::lblue << _O_LOG_INFO << color::white;
		return *this;
	}
	my_ostream& debug()
	{
		std::cout << color::lyellow << _O_LOG_DEBUG << color::white;
		return *this;
	}
	my_ostream& error()
	{
		std::cout << color::lred << _O_LOG_ERROR << color::white;
		return *this;
	}
	template<typename T> my_ostream& operator<<(const T& something)
	{
		std::cout << something;
		return *this;
	}
	typedef std::ostream& (*stream_function)(std::ostream&);
	my_ostream& operator<<(stream_function func)
	{
		func(std::cout);
		return *this;
	}
	static my_ostream& _log()
	{
		static my_ostream log_stream;
		tm timeinfo;
		char buffer[80];
		if (!OrangeLocalTime(timeinfo))
			return log_stream;
		strftime(buffer, 80, "[%Ex %EX]", &timeinfo);
		log_stream << color::lwhite << buffer << " " << color::white;
		return log_stream;
	}
};

static std::string DateTimeA()
{
	tm timeinfo;
	char buffer[80];
	if (!OrangeLocalTime(timeinfo))
		return std::string("");
	strftime(buffer, 80, "[%Ex %EX]", &timeinfo);
	return buffer;
}

#define log my_ostream::_log()
#define log_info my_ostream::_log().info()
#define log_debug my_ostream::_log().debug()
#define log_error my_ostream::_log().error()

#endif
