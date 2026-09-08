# orange_handshake: a game-less client that checks whether orange_server
# accepts players (used by the CI smoke tests, shipped with the server).

add_executable(orange_handshake handshake.cpp)
target_link_libraries(orange_handshake PRIVATE raknet Threads::Threads)
if(WIN32)
  target_link_libraries(orange_handshake PRIVATE ws2_32)
endif()
set_target_properties(orange_handshake PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${ORANGE_SERVER_OUTPUT_DIR}"
  FOLDER "server")
install(TARGETS orange_handshake RUNTIME DESTINATION server COMPONENT server)
