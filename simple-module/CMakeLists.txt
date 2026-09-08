# simple-module: minimal example of a native (C++) server module.
# It is built to make sure the module ABI keeps compiling, and installed under
# server/examples/ so it is not loaded automatically.

set(SIMPLE_MODULE_SOURCES simple-module.cpp stdafx.cpp)
if(WIN32)
  list(APPEND SIMPLE_MODULE_SOURCES dllmain.cpp)
endif()

add_library(simple-module MODULE ${SIMPLE_MODULE_SOURCES} API.h stdafx.h)
target_include_directories(simple-module PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${ORANGE_SHARED_DIR}")
set_target_properties(simple-module PROPERTIES
  PREFIX ""
  OUTPUT_NAME "simple-module"
  LIBRARY_OUTPUT_DIRECTORY "${ORANGE_SERVER_OUTPUT_DIR}/examples/modules"
  FOLDER "server")

install(TARGETS simple-module LIBRARY DESTINATION server/examples/modules COMPONENT server)
