# Launcher.exe: starts GTA V (or attaches to a running GTA5.exe with --inject),
# keeps the client package up to date from GitHub releases and injects
# orange-core.dll. Windows / MSVC only.

add_executable(Launcher WIN32
  FolderBrowser.cpp
  Injector.cpp
  Launcher.cpp
  LauncherLog.cpp
  NativesCrossmap.cpp
  Registry.cpp
  Updater.cpp
  Utils.cpp
  stdafx.cpp
  Launcher.rc
  FolderBrowser.h Injector.h Launcher.h LauncherLog.h NativesCrossmap.h Registry.h
  UpdateManifest.h Updater.h Utils.h resource.h stdafx.h targetver.h)

target_include_directories(Launcher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${ORANGE_SHARED_DIR}")
target_compile_definitions(Launcher PRIVATE _WINDOWS UNICODE _UNICODE
  "$<$<COMPILE_LANGUAGE:C,CXX>:ORANGE_VERSION=\"${ORANGE_VERSION_STRING}\">")
target_link_libraries(Launcher PRIVATE tinyxml2 gdiplus shell32 ole32 user32 gdi32 advapi32 psapi shlwapi winhttp bcrypt version)

set_target_properties(Launcher PROPERTIES
  OUTPUT_NAME "Launcher"
  RUNTIME_OUTPUT_DIRECTORY "${ORANGE_CLIENT_OUTPUT_DIR}"
  FOLDER "client")

install(TARGETS Launcher RUNTIME DESTINATION client COMPONENT client)
