# Vendored third-party libraries, all built as static libraries.

set(ORANGE_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(MSVC)
  set(_deps_quiet /W0)
else()
  set(_deps_quiet -w)
endif()

# --- RakNet -------------------------------------------------------------------
file(GLOB RAKNET_SOURCES "${ORANGE_DEPS_DIR}/RakNet/Source/*.cpp")
file(GLOB RAKNET_HEADERS "${ORANGE_DEPS_DIR}/RakNet/Source/*.h")
add_library(raknet STATIC ${RAKNET_SOURCES} ${RAKNET_HEADERS})
target_include_directories(raknet PUBLIC "${ORANGE_DEPS_DIR}/RakNet/Source")
target_compile_definitions(raknet PUBLIC _RAKNET_LIB)
target_compile_options(raknet PRIVATE ${_deps_quiet})
if(WIN32)
  target_link_libraries(raknet PUBLIC ws2_32)
else()
  target_link_libraries(raknet PUBLIC Threads::Threads)
endif()
set_target_properties(raknet PROPERTIES FOLDER "deps")

# --- tinyxml2 -----------------------------------------------------------------
add_library(tinyxml2 STATIC "${ORANGE_DEPS_DIR}/tinyxml2/tinyxml2.cpp" "${ORANGE_DEPS_DIR}/tinyxml2/tinyxml2.h")
target_include_directories(tinyxml2 PUBLIC "${ORANGE_DEPS_DIR}/tinyxml2")
target_compile_options(tinyxml2 PRIVATE ${_deps_quiet})
set_target_properties(tinyxml2 PROPERTIES FOLDER "deps")

# --- yaml-cpp 0.3 (old API) ---------------------------------------------------
file(GLOB YAMLCPP_SOURCES
  "${ORANGE_DEPS_DIR}/ServerDeps/yaml-cpp/src/*.cpp"
  "${ORANGE_DEPS_DIR}/ServerDeps/yaml-cpp/src/contrib/*.cpp")
add_library(yamlcpp STATIC ${YAMLCPP_SOURCES})
target_include_directories(yamlcpp
  PUBLIC "${ORANGE_DEPS_DIR}/ServerDeps/yaml-cpp/include"
  PRIVATE "${ORANGE_DEPS_DIR}/ServerDeps/yaml-cpp/src")
target_compile_options(yamlcpp PRIVATE ${_deps_quiet})
if(MSVC)
  # yaml-cpp 0.3 still uses std::auto_ptr in its public headers
  target_compile_definitions(yamlcpp PUBLIC _HAS_AUTO_PTR_ETC=1)
endif()
set_target_properties(yamlcpp PROPERTIES FOLDER "deps")

# --- civetweb 1.8 (embedded HTTP server) --------------------------------------
add_library(civetweb STATIC
  "${ORANGE_DEPS_DIR}/ServerDeps/Civet/civetweb.c"
  "${ORANGE_DEPS_DIR}/ServerDeps/Civet/CivetServer.cpp")
target_include_directories(civetweb PUBLIC "${ORANGE_DEPS_DIR}/ServerDeps/Civet")
target_compile_definitions(civetweb PRIVATE NO_SSL NO_CGI USE_IPV6)
target_compile_options(civetweb PRIVATE ${_deps_quiet})
if(WIN32)
  target_link_libraries(civetweb PUBLIC ws2_32 comdlg32)
else()
  target_link_libraries(civetweb PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()
set_target_properties(civetweb PROPERTIES FOLDER "deps")

# --- LuaJIT -------------------------------------------------------------------
include("${CMAKE_SOURCE_DIR}/cmake/LuaJIT.cmake")
