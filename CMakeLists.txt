cmake_minimum_required(VERSION 3.16)

project(GTAOrange
  VERSION 0.2.0
  DESCRIPTION "GTA:Orange - an alternative multiplayer modification for Grand Theft Auto V"
  LANGUAGES C CXX)

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------
option(ORANGE_BUILD_SERVER "Build the dedicated server and the server modules (all platforms)" ON)
if(WIN32 AND MSVC)
  set(_orange_client_default ON)
else()
  set(_orange_client_default OFF)
endif()
option(ORANGE_BUILD_CLIENT "Build the game client: OrangeLauncher.exe + orange-core.dll (Windows / MSVC only)" ${_orange_client_default})
option(ORANGE_BUILD_TOOLS "Build helper tools (font-converter, luajit interpreter)" ON)
option(ORANGE_LUA_MYSQL "Enable MySQL support in lua-module when a MySQL/MariaDB client library is found" ON)
option(ORANGE_ENABLE_SCALEFORM "Compile the experimental Scaleform DrawText code into orange-core (needs the proprietary GFx SDK libraries)" OFF)
option(ORANGE_BUILD_TESTS "Build the unit tests (ctest)" ON)

# Version string embedded in the binaries and used by the auto-updater. CI sets
# it to the release tag (e.g. 0.2.0) or to a nightly id (nightly-YYYYMMDD-sha);
# local builds are "<version>-dev" and are never replaced by the updater.
set(ORANGE_VERSION_STRING "${PROJECT_VERSION}-dev" CACHE STRING "Version string embedded in the binaries")
file(WRITE "${CMAKE_BINARY_DIR}/version.txt" "${ORANGE_VERSION_STRING}\n")

if(ORANGE_BUILD_CLIENT AND NOT (WIN32 AND MSVC))
  message(FATAL_ERROR "The client (orange-core.dll / OrangeLauncher.exe) can only be built with MSVC on Windows. Configure with -DORANGE_BUILD_CLIENT=OFF.")
endif()

# ------------------------------------------------------------------------------
# Global settings
# ------------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(WIN32)
  enable_language(RC)    # icon / version resources
else()
  enable_language(ASM)   # LuaJIT VM (lj_vm.S)
endif()

find_package(Threads REQUIRED)

if(MSVC)
  # Static CRT: the client DLL gets injected into GTA5.exe and must not depend
  # on a VC++ redistributable being installed (also true inside a Proton prefix).
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_definitions(
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_DEPRECATE
    _WINSOCK_DEPRECATED_NO_WARNINGS
    _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
  # Only for C/C++: CMake would otherwise hand these to rc.exe as well.
  # (C4838: the embedded Lua bytecode arrays trigger it a few thousand times)
  foreach(_flag /bigobj /wd4996 /wd4267 /wd4244 /wd4800 /wd4305 /wd4838 /wd4309)
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:${_flag}>")
  endforeach()
  if(CMAKE_GENERATOR MATCHES "Visual Studio")
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:/MP>")
  endif()
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(WARNING "GTA:Orange targets 64-bit builds; 32-bit is unsupported.")
  endif()
else()
  foreach(_flag -Wall
      -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function
      -Wno-sign-compare -Wno-reorder -Wno-unknown-pragmas -Wno-deprecated-declarations
      -Wno-misleading-indentation -Wno-parentheses -Wno-unused-result)
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:${_flag}>")
  endforeach()
endif()

set(ORANGE_SERVER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/server")
set(ORANGE_CLIENT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/client")
set(ORANGE_SHARED_DIR "${CMAKE_SOURCE_DIR}/shared")

# ------------------------------------------------------------------------------
# Dependencies (vendored, static)
# ------------------------------------------------------------------------------
add_subdirectory(deps)

# ------------------------------------------------------------------------------
# Server
# ------------------------------------------------------------------------------
if(ORANGE_BUILD_SERVER)
  add_subdirectory(Server)
  add_subdirectory(lua-module)
  add_subdirectory(simple-module)

  # runtime files (sample config, Lua API bootstrap, example resource)
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/runtime/server/" DESTINATION server COMPONENT server)
  install(FILES "${CMAKE_BINARY_DIR}/version.txt" DESTINATION server COMPONENT server)
  install(PROGRAMS "${CMAKE_SOURCE_DIR}/tools/update-server.sh" DESTINATION server COMPONENT server)
  install(FILES "${CMAKE_SOURCE_DIR}/tools/update-server.ps1" DESTINATION server COMPONENT server)
  add_custom_target(orange_server_runtime ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/runtime/server" "${ORANGE_SERVER_OUTPUT_DIR}"
    COMMENT "Copying server runtime files to ${ORANGE_SERVER_OUTPUT_DIR}")
  set_target_properties(orange_server_runtime PROPERTIES FOLDER "server")
endif()

# ------------------------------------------------------------------------------
# Client (Windows only)
# ------------------------------------------------------------------------------
if(ORANGE_BUILD_CLIENT)
  add_subdirectory(orange-core)
  add_subdirectory(Launcher)
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/runtime/client/" DESTINATION client COMPONENT client)
  install(FILES "${CMAKE_BINARY_DIR}/version.txt" DESTINATION client COMPONENT client)
  add_custom_target(orange_client_runtime ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/runtime/client" "${ORANGE_CLIENT_OUTPUT_DIR}"
    COMMENT "Copying client runtime files to ${ORANGE_CLIENT_OUTPUT_DIR}")
  set_target_properties(orange_client_runtime PROPERTIES FOLDER "client")
endif()

# ------------------------------------------------------------------------------
# Tools & tests
# ------------------------------------------------------------------------------
if(ORANGE_BUILD_TOOLS)
  add_subdirectory(font-converter)
endif()
if(ORANGE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------
message(STATUS "GTA:Orange ${PROJECT_VERSION} (version string: ${ORANGE_VERSION_STRING})")
message(STATUS "  Build type ........ ${CMAKE_BUILD_TYPE}")
message(STATUS "  Server ............ ${ORANGE_BUILD_SERVER}")
message(STATUS "  Client ............ ${ORANGE_BUILD_CLIENT}")
message(STATUS "  Tools ............. ${ORANGE_BUILD_TOOLS}")
if(ORANGE_BUILD_SERVER)
  message(STATUS "  lua-module MySQL .. ${ORANGE_LUA_MYSQL_ENABLED}")
endif()
if(ORANGE_BUILD_CLIENT)
  message(STATUS "  Scaleform ......... ${ORANGE_ENABLE_SCALEFORM}")
endif()
