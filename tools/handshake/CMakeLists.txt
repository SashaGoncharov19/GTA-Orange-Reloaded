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

# orange_bot: N simulated players (load and streaming tests), shipped with the server.
add_executable(orange_bot bot.cpp)
target_include_directories(orange_bot PRIVATE "${ORANGE_SHARED_DIR}")
target_compile_definitions(orange_bot PRIVATE "$<$<COMPILE_LANGUAGE:C,CXX>:ORANGE_VERSION=\"${ORANGE_VERSION_STRING}\">")
target_link_libraries(orange_bot PRIVATE raknet Threads::Threads)
if(WIN32)
  target_link_libraries(orange_bot PRIVATE ws2_32)
  target_compile_definitions(orange_bot PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
set_target_properties(orange_bot PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${ORANGE_SERVER_OUTPUT_DIR}"
  FOLDER "server")
install(TARGETS orange_bot RUNTIME DESTINATION server COMPONENT server)
