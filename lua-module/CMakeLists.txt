# lua-module: LuaJIT scripting support for the server (loaded from server/modules/)

set(LUA_MODULE_SOURCES
  SResource.cpp
  ls_mysql.cpp
  lua-module.cpp
  luasql.cpp
  lua_Blips.cpp
  lua_Main.cpp
  lua_Markers.cpp
  lua_Object.cpp
  lua_Player.cpp
  lua_Vehicle.cpp
  stdafx.cpp)
set(LUA_MODULE_HEADERS API.h SResource.h lua_Main.h luasql.h stdafx.h)
if(WIN32)
  list(APPEND LUA_MODULE_SOURCES dllmain.cpp)
endif()

add_library(lua-module MODULE ${LUA_MODULE_SOURCES} ${LUA_MODULE_HEADERS})

target_include_directories(lua-module PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${ORANGE_SHARED_DIR}")
target_link_libraries(lua-module PRIVATE luajit)

# --- optional MySQL support (LuaSQL) ------------------------------------------
set(ORANGE_LUA_MYSQL_ENABLED OFF)
if(ORANGE_LUA_MYSQL)
  find_path(MYSQL_INCLUDE_DIR NAMES mysql.h PATH_SUFFIXES mysql mariadb)
  find_library(MYSQL_LIBRARY NAMES mysqlclient mariadb libmysql libmariadb)
  if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
    set(ORANGE_LUA_MYSQL_ENABLED ON)
    target_include_directories(lua-module PRIVATE "${MYSQL_INCLUDE_DIR}")
    target_link_libraries(lua-module PRIVATE "${MYSQL_LIBRARY}")
    message(STATUS "lua-module: MySQL support enabled (${MYSQL_LIBRARY})")
  else()
    message(STATUS "lua-module: MySQL client library not found, building without SQL support")
  endif()
endif()
if(NOT ORANGE_LUA_MYSQL_ENABLED)
  target_compile_definitions(lua-module PRIVATE _LUA_NOSQL)
endif()
set(ORANGE_LUA_MYSQL_ENABLED ${ORANGE_LUA_MYSQL_ENABLED} PARENT_SCOPE)

set_target_properties(lua-module PROPERTIES
  PREFIX ""
  OUTPUT_NAME "lua-module"
  LIBRARY_OUTPUT_DIRECTORY "${ORANGE_SERVER_OUTPUT_DIR}/modules"
  FOLDER "server")

install(TARGETS lua-module LIBRARY DESTINATION server/modules COMPONENT server)
