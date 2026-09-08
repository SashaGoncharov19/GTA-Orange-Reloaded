#include "stdafx.h"
#ifdef ORANGE_MYSQL_DLOPEN
#include "MySQLDyn.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>

MySQLDynApi g_mysqlDyn = {};

static void * g_mysqlHandle = nullptr;
static std::string g_mysqlError;

// MySQL 8.0 = .21, 8.1-8.3 = .22/.23, 8.4 and 9.x = .24; MariaDB Connector/C = libmariadb.so.3.
static const char * const kCandidates[] = {
	"libmysqlclient.so.21", "libmysqlclient.so.24", "libmysqlclient.so.23", "libmysqlclient.so.22",
	"libmysqlclient.so.20", "libmariadb.so.3", "libmysqlclient.so", "libmariadb.so", nullptr
};

template<typename T>
static bool Resolve(void * handle, const char * name, T & out, std::string & error)
{
	void * symbol = dlsym(handle, name);
	if (!symbol)
	{
		error = std::string("the MySQL client library has no ") + name;
		return false;
	}
	memcpy(&out, &symbol, sizeof(out));
	return true;
}

bool MySQLDynLoad(std::string & error)
{
	if (g_mysqlHandle)
		return true;
	if (!g_mysqlError.empty())
	{
		error = g_mysqlError;
		return false;
	}

	std::string tried;
	void * handle = nullptr;
	const char * forced = getenv("ORANGE_MYSQL_LIBRARY");
	if (forced && *forced)
	{
		handle = dlopen(forced, RTLD_NOW | RTLD_LOCAL);
		tried = forced;
	}
	else
	{
		for (const char * const * name = kCandidates; *name && !handle; ++name)
		{
			handle = dlopen(*name, RTLD_NOW | RTLD_LOCAL);
			if (!tried.empty())
				tried += ", ";
			tried += *name;
		}
	}
	if (!handle)
	{
		const char * why = dlerror();
		g_mysqlError = "no MySQL/MariaDB client library found (tried " + tried + "; last error: " + (why ? why : "none")
			+ "). Install libmysqlclient21 (Ubuntu) or libmariadb3 (Debian), or set ORANGE_MYSQL_LIBRARY=/path/to/the/library";
		error = g_mysqlError;
		return false;
	}

	MySQLDynApi api = {};
	bool ok = Resolve(handle, "mysql_init", api.init, error)
		&& Resolve(handle, "mysql_real_connect", api.real_connect, error)
		&& Resolve(handle, "mysql_real_query", api.real_query, error)
		&& Resolve(handle, "mysql_store_result", api.store_result, error)
		&& Resolve(handle, "mysql_field_count", api.field_count, error)
		&& Resolve(handle, "mysql_affected_rows", api.affected_rows, error)
		&& Resolve(handle, "mysql_insert_id", api.insert_id, error)
		&& Resolve(handle, "mysql_error", api.error, error)
		&& Resolve(handle, "mysql_close", api.close, error)
		&& Resolve(handle, "mysql_real_escape_string", api.real_escape_string, error)
		&& Resolve(handle, "mysql_fetch_fields", api.fetch_fields, error)
		&& Resolve(handle, "mysql_fetch_row", api.fetch_row, error)
		&& Resolve(handle, "mysql_fetch_lengths", api.fetch_lengths, error)
		&& Resolve(handle, "mysql_num_rows", api.num_rows, error)
		&& Resolve(handle, "mysql_free_result", api.free_result, error)
		&& Resolve(handle, "mysql_commit", api.commit, error)
		&& Resolve(handle, "mysql_rollback", api.rollback, error)
		&& Resolve(handle, "mysql_autocommit", api.autocommit, error);
	if (!ok)
	{
		dlclose(handle);
		g_mysqlError = error;
		return false;
	}
	g_mysqlDyn = api;
	g_mysqlHandle = handle;
	return true;
}
#endif
