# GTA:Orange dedicated server

set(ORANGE_SERVER_SOURCES
  API.cpp
  CClientScripting.cpp
  CConfig.cpp
  CHTTPServer.cpp
  CNetwokMarker.cpp
  CNetwork3DText.cpp
  CNetworkBlip.cpp
  CNetworkConnection.cpp
  CNetworkObject.cpp
  CNetworkPlayer.cpp
  CNetworkVehicle.cpp
  CRPCPlugin.cpp
  Plugin.cpp
  main.cpp
  stdafx.cpp)

set(ORANGE_SERVER_HEADERS
  API.h CClientScripting.h CConfig.h CHTTPServer.h CLog.h CNetwork3DText.h
  CNetworkBlip.h CNetworkConnection.h CNetworkMarker.h CNetworkObject.h
  CNetworkPlayer.h CNetworkVehicle.h CRPCPlugin.h Plugin.h stdafx.h)

if(WIN32)
  list(APPEND ORANGE_SERVER_SOURCES resources.rc)
endif()

add_executable(orange_server ${ORANGE_SERVER_SOURCES} ${ORANGE_SERVER_HEADERS})

target_include_directories(orange_server PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${ORANGE_SHARED_DIR}")

target_compile_definitions(orange_server PRIVATE
  _SERVER
  PROJECT_NAME="orange_server")

target_link_libraries(orange_server PRIVATE raknet yamlcpp civetweb Threads::Threads ${CMAKE_DL_LIBS})
if(WIN32)
  target_link_libraries(orange_server PRIVATE winmm ws2_32)
endif()

set_target_properties(orange_server PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${ORANGE_SERVER_OUTPUT_DIR}"
  FOLDER "server")

install(TARGETS orange_server RUNTIME DESTINATION server COMPONENT server)
