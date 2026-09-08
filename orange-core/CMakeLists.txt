# orange-core.dll: the client core that gets injected into GTA5.exe.
# Windows / MSVC only.

set(ORANGE_CORE_SOURCES
  BackScene.cpp
  ChatGUI.cpp
  Core/Chat.cpp
  Core/Commands.cpp
  Core/nativeInvoker.cpp
  Core/scrEngine.cpp
  Core/Script.cpp
  Core/ScriptInvoker.cpp
  Core/scrManager.cpp
  Core/scrThread.cpp
  D3D11/d3dhook.cpp
  D3D11/imgui.cpp
  D3D11/imgui_draw.cpp
  D3D11/imgui_impl_dx11.cpp
  D3D11/Memory/GameMemory.cpp
  D3D11/Memory/HMemory.cpp
  D3D11/Memory/Hooking.cpp
  D3D11/Memory/minhook/src/buffer.cpp
  D3D11/Memory/minhook/src/HDE/hde64.cpp
  D3D11/Memory/minhook/src/hook.cpp
  D3D11/Memory/minhook/src/trampoline.cpp
  D3D11/rendering.cpp
  dllmain.cpp
  Globals.cpp
  GtaScripts.cpp
  GTA/CGraphics.cpp
  GTA/CRage.cpp
  GTA/CReplayInterface.cpp
  GTA/sysAllocator.cpp
  GTA/VTasks.cpp
  GuiDispatcher.cpp
  HWID.cpp
  LuaNativeEngine.cpp
  md5.cpp
  Memory.cpp
  MemoryCache.cpp
  Network/CConfig.cpp
  Network/CEntity.cpp
  Network/CLocalPlayer.cpp
  Network/CNetwork3DText.cpp
  Network/CNetworkBlip.cpp
  Network/CNetworkConnection.cpp
  Network/CNetworkMarker.cpp
  Network/CNetworkObject.cpp
  Network/CNetworkPlayer.cpp
  Network/CNetworkUI.cpp
  Network/CNetworkVehicle.cpp
  Network/CPedestrian.cpp
  Network/CRPCPlugin.cpp
  Network/CVehicle.cpp
  Network/Functions.cpp
  orange-core.cpp
  Registry.cpp
  ScriptingScene.cpp
  Scripting/CScriptEngine.cpp
  Scripting/LuaMainDefs.cpp
  Scripts/ChatScript.cpp
  Scripts/GameInit.cpp
  Scripts/NetworkScript.cpp
  Scripts/ScriptDispatcher.cpp
  ServerBrowser.cpp
  stdafx.cpp
  Utils.cpp
  orange-core.rc)

add_library(orange-core SHARED ${ORANGE_CORE_SOURCES})

target_include_directories(orange-core PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${ORANGE_SHARED_DIR}"
  "${CMAKE_SOURCE_DIR}/deps/RakNet/DependentExtensions"
  "${CMAKE_SOURCE_DIR}/deps/RakNet/DependentExtensions/portaudio_v18_1/pa_common"
  "${CMAKE_SOURCE_DIR}/deps/RakNet/DependentExtensions/portaudio_v18_1/pa_win_wmme")

target_compile_definitions(orange-core PRIVATE
  _WINDOWS _USRDLL ORANGECORE_EXPORTS UNICODE _UNICODE
  "$<$<COMPILE_LANGUAGE:C,CXX>:ORANGE_VERSION=\"${ORANGE_VERSION_STRING}\">")

target_link_libraries(orange-core PRIVATE
  raknet tinyxml2 luajit
  d3dcompiler dxguid winmm version iphlpapi
  user32 gdi32 shell32 ole32 advapi32 psapi imm32)

# Experimental Scaleform (GFx) DrawText support. The headers and sources of the
# GFx SDK are vendored under deps/ClientDeps/Scaleform, but the SDK libraries
# (libgfx.lib) are proprietary and not part of this repository.
if(ORANGE_ENABLE_SCALEFORM)
  set(ORANGE_SCALEFORM_LIBRARY "" CACHE FILEPATH "Path to Scaleform GFx SDK 4.0 libgfx.lib (x64, static CRT)")
  target_compile_definitions(orange-core PRIVATE ORANGE_WITH_SCALEFORM GFX_ENABLE_DRAWTEXT=1)
  target_include_directories(orange-core PRIVATE
    "${CMAKE_SOURCE_DIR}/deps/ClientDeps/Scaleform/Include"
    "${CMAKE_SOURCE_DIR}/deps/ClientDeps/Scaleform/Src")
  if(ORANGE_SCALEFORM_LIBRARY)
    target_link_libraries(orange-core PRIVATE "${ORANGE_SCALEFORM_LIBRARY}")
  else()
    message(WARNING "ORANGE_ENABLE_SCALEFORM is ON but ORANGE_SCALEFORM_LIBRARY is not set; linking may fail.")
  endif()
endif()

set_target_properties(orange-core PROPERTIES
  PREFIX ""
  OUTPUT_NAME "orange-core"
  RUNTIME_OUTPUT_DIRECTORY "${ORANGE_CLIENT_OUTPUT_DIR}"
  FOLDER "client")

install(TARGETS orange-core RUNTIME DESTINATION client COMPONENT client)
