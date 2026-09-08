#pragma once
// MySQL client library loaded at run time (Linux).
//
// The Linux server package is built on Ubuntu 22.04 against libmysqlclient
// 8.0. Linking it directly made lua-module.so depend on libmysqlclient.so.21,
// which Debian, Fedora, Arch or a bare container do not have under that name
// (Debian ships MariaDB's libmariadb.so.3), so the whole Lua module failed to
// load and no resource started. Now the module loads without any MySQL
// library; the first luasql connection dlopen()s whichever client library
// is installed and resolves the functions the LuaSQL driver uses.
//
// ORANGE_MYSQL_LIBRARY=/path/to/lib.so overrides the search list.
#include <mysql.h>
#include <string>

struct MySQLDynApi
{
	decltype(&mysql_init) init;
	decltype(&mysql_real_connect) real_connect;
	decltype(&mysql_real_query) real_query;
	decltype(&mysql_store_result) store_result;
	decltype(&mysql_field_count) field_count;
	decltype(&mysql_affected_rows) affected_rows;
	decltype(&mysql_insert_id) insert_id;
	decltype(&mysql_error) error;
	decltype(&mysql_close) close;
	decltype(&mysql_real_escape_string) real_escape_string;
	decltype(&mysql_fetch_fields) fetch_fields;
	decltype(&mysql_fetch_row) fetch_row;
	decltype(&mysql_fetch_lengths) fetch_lengths;
	decltype(&mysql_num_rows) num_rows;
	decltype(&mysql_free_result) free_result;
	decltype(&mysql_commit) commit;
	decltype(&mysql_rollback) rollback;
	decltype(&mysql_autocommit) autocommit;
};

extern MySQLDynApi g_mysqlDyn;

// Loads the client library once. Returns false with a description (which
// names were tried, or which symbol is missing) in `error`.
bool MySQLDynLoad(std::string & error);

// From here on the driver's mysql_* calls go through the loaded pointers.
#define mysql_init g_mysqlDyn.init
#define mysql_real_connect g_mysqlDyn.real_connect
#define mysql_real_query g_mysqlDyn.real_query
#define mysql_store_result g_mysqlDyn.store_result
#define mysql_field_count g_mysqlDyn.field_count
#define mysql_affected_rows g_mysqlDyn.affected_rows
#define mysql_insert_id g_mysqlDyn.insert_id
#define mysql_error g_mysqlDyn.error
#define mysql_close g_mysqlDyn.close
#define mysql_real_escape_string g_mysqlDyn.real_escape_string
#define mysql_fetch_fields g_mysqlDyn.fetch_fields
#define mysql_fetch_row g_mysqlDyn.fetch_row
#define mysql_fetch_lengths g_mysqlDyn.fetch_lengths
#define mysql_num_rows g_mysqlDyn.num_rows
#define mysql_free_result g_mysqlDyn.free_result
#define mysql_commit g_mysqlDyn.commit
#define mysql_rollback g_mysqlDyn.rollback
#define mysql_autocommit g_mysqlDyn.autocommit
