# Builds the vendored LuaJIT (deps/ServerDeps/LuaJIT-2.1.0-beta2) as a static
# library target called `luajit`, replicating what src/Makefile and
# src/msvcbuild.bat do:
#
#   1. build host tool `minilua`
#   2. run DynASM (minilua + dynasm.lua) on vm_<arch>.dasc -> buildvm_arch.h
#   3. build host tool `buildvm`
#   4. run buildvm to generate lj_vm.S / lj_vm.obj and the lj_*def.h headers
#   5. compile lj_*.c + lib_*.c + the generated VM into libluajit
#
# Only native (non cross-compiling) x86/x64 builds are supported, which is all
# GTA:Orange needs (Windows x64 client, x64 Linux/Windows server).

set(LUAJIT_DIR "${CMAKE_SOURCE_DIR}/deps/ServerDeps/LuaJIT-2.1.0-beta2")
set(LUAJIT_SRC "${LUAJIT_DIR}/src")
set(LUAJIT_DYNASM "${LUAJIT_DIR}/dynasm")
set(LUAJIT_GEN "${CMAKE_BINARY_DIR}/luajit-gen")
file(MAKE_DIRECTORY "${LUAJIT_GEN}/jit")

option(LUAJIT_ENABLE_GC64 "Build LuaJIT in GC64 mode (x64 only, disables the JIT compiler in 2.1.0-beta2)" OFF)

if(CMAKE_CROSSCOMPILING)
  message(FATAL_ERROR "LuaJIT.cmake does not support cross-compiling")
endif()

# --- architecture -----------------------------------------------------------
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _lj_proc)
if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND _lj_proc MATCHES "^(x86_64|amd64|x64)$")
  set(LUAJIT_ARCH x64)
  set(LUAJIT_DASM_FLAGS -D ENDIAN_LE -D P64 -D JIT -D FFI -D FPU -D HFABI)
  if(LUAJIT_ENABLE_GC64)
    set(LUAJIT_DASC "${LUAJIT_SRC}/vm_x64.dasc")
  else()
    set(LUAJIT_DASC "${LUAJIT_SRC}/vm_x86.dasc")
  endif()
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4 AND _lj_proc MATCHES "^(x86|i[3-6]86|amd64|x86_64)$")
  set(LUAJIT_ARCH x86)
  set(LUAJIT_DASM_FLAGS -D ENDIAN_LE -D JIT -D FFI -D FPU -D HFABI)
  set(LUAJIT_DASC "${LUAJIT_SRC}/vm_x86.dasc")
else()
  message(FATAL_ERROR "LuaJIT.cmake: unsupported target architecture '${CMAKE_SYSTEM_PROCESSOR}' (${CMAKE_SIZEOF_VOID_P}*8 bit)")
endif()
string(TOUPPER "${LUAJIT_ARCH}" _lj_arch_upper)

if(WIN32)
  list(APPEND LUAJIT_DASM_FLAGS -D WIN)
endif()

set(LUAJIT_ARCH_DEFS LUAJIT_TARGET=LUAJIT_ARCH_${_lj_arch_upper} LJ_ARCH_HASFPU=1 LJ_ABI_SOFTFP=0)
if(LUAJIT_ENABLE_GC64)
  list(APPEND LUAJIT_ARCH_DEFS LUAJIT_ENABLE_GC64)
endif()

# --- compiler flags -----------------------------------------------------------
if(MSVC)
  set(LUAJIT_C_FLAGS /O2 /W3 /wd4996)
  set(LUAJIT_C_DEFS _CRT_SECURE_NO_DEPRECATE)
  if(LUAJIT_ARCH STREQUAL "x86")
    list(APPEND LUAJIT_C_FLAGS /arch:SSE2)
  endif()
else()
  set(LUAJIT_C_FLAGS -O2 -fomit-frame-pointer -Wall -fno-stack-protector -U_FORTIFY_SOURCE)
  set(LUAJIT_C_DEFS _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE)
  if(LUAJIT_ARCH STREQUAL "x86")
    list(APPEND LUAJIT_C_FLAGS -march=i686 -msse -msse2 -mfpmath=sse)
  endif()
endif()

# --- host tool: minilua -------------------------------------------------------
add_executable(luajit-minilua "${LUAJIT_SRC}/host/minilua.c")
target_compile_options(luajit-minilua PRIVATE ${LUAJIT_C_FLAGS})
target_compile_definitions(luajit-minilua PRIVATE ${LUAJIT_C_DEFS})
if(NOT MSVC)
  target_link_libraries(luajit-minilua PRIVATE m)
endif()
set_target_properties(luajit-minilua PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${LUAJIT_GEN}/host" FOLDER "deps/luajit")

# --- DynASM: vm_<arch>.dasc -> buildvm_arch.h ---------------------------------
file(GLOB _lj_dynasm_lua "${LUAJIT_DYNASM}/*.lua")
add_custom_command(
  OUTPUT "${LUAJIT_GEN}/buildvm_arch.h"
  COMMAND luajit-minilua "${LUAJIT_DYNASM}/dynasm.lua" -LN ${LUAJIT_DASM_FLAGS} -o "${LUAJIT_GEN}/buildvm_arch.h" "${LUAJIT_DASC}"
  DEPENDS luajit-minilua "${LUAJIT_DASC}" ${_lj_dynasm_lua}
  WORKING_DIRECTORY "${LUAJIT_SRC}"
  COMMENT "DYNASM    buildvm_arch.h (${LUAJIT_ARCH})"
  VERBATIM)

# --- host tool: buildvm -------------------------------------------------------
add_executable(luajit-buildvm
  "${LUAJIT_SRC}/host/buildvm.c"
  "${LUAJIT_SRC}/host/buildvm_asm.c"
  "${LUAJIT_SRC}/host/buildvm_peobj.c"
  "${LUAJIT_SRC}/host/buildvm_lib.c"
  "${LUAJIT_SRC}/host/buildvm_fold.c"
  "${LUAJIT_GEN}/buildvm_arch.h")
target_include_directories(luajit-buildvm PRIVATE "${LUAJIT_GEN}" "${LUAJIT_SRC}" "${LUAJIT_DYNASM}")
target_compile_options(luajit-buildvm PRIVATE ${LUAJIT_C_FLAGS})
target_compile_definitions(luajit-buildvm PRIVATE ${LUAJIT_C_DEFS} ${LUAJIT_ARCH_DEFS})
set_target_properties(luajit-buildvm PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${LUAJIT_GEN}/host" FOLDER "deps/luajit")

# --- generated headers --------------------------------------------------------
set(LUAJIT_LJLIB_C
  "${LUAJIT_SRC}/lib_base.c" "${LUAJIT_SRC}/lib_math.c" "${LUAJIT_SRC}/lib_bit.c"
  "${LUAJIT_SRC}/lib_string.c" "${LUAJIT_SRC}/lib_table.c" "${LUAJIT_SRC}/lib_io.c"
  "${LUAJIT_SRC}/lib_os.c" "${LUAJIT_SRC}/lib_package.c" "${LUAJIT_SRC}/lib_debug.c"
  "${LUAJIT_SRC}/lib_jit.c" "${LUAJIT_SRC}/lib_ffi.c")

set(LUAJIT_GENERATED_HEADERS)
foreach(_kind bcdef ffdef libdef recdef)
  add_custom_command(
    OUTPUT "${LUAJIT_GEN}/lj_${_kind}.h"
    COMMAND luajit-buildvm -m ${_kind} -o "${LUAJIT_GEN}/lj_${_kind}.h" ${LUAJIT_LJLIB_C}
    DEPENDS luajit-buildvm ${LUAJIT_LJLIB_C}
    WORKING_DIRECTORY "${LUAJIT_SRC}"
    COMMENT "BUILDVM   lj_${_kind}.h"
    VERBATIM)
  list(APPEND LUAJIT_GENERATED_HEADERS "${LUAJIT_GEN}/lj_${_kind}.h")
endforeach()

add_custom_command(
  OUTPUT "${LUAJIT_GEN}/lj_folddef.h"
  COMMAND luajit-buildvm -m folddef -o "${LUAJIT_GEN}/lj_folddef.h" "${LUAJIT_SRC}/lj_opt_fold.c"
  DEPENDS luajit-buildvm "${LUAJIT_SRC}/lj_opt_fold.c"
  WORKING_DIRECTORY "${LUAJIT_SRC}"
  COMMENT "BUILDVM   lj_folddef.h"
  VERBATIM)
list(APPEND LUAJIT_GENERATED_HEADERS "${LUAJIT_GEN}/lj_folddef.h")

add_custom_command(
  OUTPUT "${LUAJIT_GEN}/jit/vmdef.lua"
  COMMAND luajit-buildvm -m vmdef -o "${LUAJIT_GEN}/jit/vmdef.lua" ${LUAJIT_LJLIB_C}
  DEPENDS luajit-buildvm ${LUAJIT_LJLIB_C}
  WORKING_DIRECTORY "${LUAJIT_SRC}"
  COMMENT "BUILDVM   jit/vmdef.lua"
  VERBATIM)

# --- the VM itself ------------------------------------------------------------
if(MSVC)
  set(LUAJIT_VM_OBJECT "${LUAJIT_GEN}/lj_vm.obj")
  set(_lj_vm_mode peobj)
elseif(APPLE)
  set(LUAJIT_VM_OBJECT "${LUAJIT_GEN}/lj_vm.S")
  set(_lj_vm_mode machasm)
elseif(WIN32)
  set(LUAJIT_VM_OBJECT "${LUAJIT_GEN}/lj_vm.S")
  set(_lj_vm_mode coffasm)
else()
  set(LUAJIT_VM_OBJECT "${LUAJIT_GEN}/lj_vm.S")
  set(_lj_vm_mode elfasm)
endif()
add_custom_command(
  OUTPUT "${LUAJIT_VM_OBJECT}"
  COMMAND luajit-buildvm -m ${_lj_vm_mode} -o "${LUAJIT_VM_OBJECT}"
  DEPENDS luajit-buildvm
  WORKING_DIRECTORY "${LUAJIT_SRC}"
  COMMENT "BUILDVM   lj_vm (${_lj_vm_mode})"
  VERBATIM)
if(MSVC)
  set_source_files_properties("${LUAJIT_VM_OBJECT}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
else()
  set_source_files_properties("${LUAJIT_VM_OBJECT}" PROPERTIES GENERATED TRUE LANGUAGE ASM)
endif()

# --- the library --------------------------------------------------------------
file(GLOB LUAJIT_CORE_SOURCES "${LUAJIT_SRC}/lj_*.c" "${LUAJIT_SRC}/lib_*.c")
add_library(luajit STATIC
  ${LUAJIT_CORE_SOURCES}
  ${LUAJIT_GENERATED_HEADERS}
  "${LUAJIT_GEN}/jit/vmdef.lua"
  "${LUAJIT_VM_OBJECT}")
target_include_directories(luajit PUBLIC "${LUAJIT_SRC}" PRIVATE "${LUAJIT_GEN}")
target_compile_options(luajit PRIVATE ${LUAJIT_C_FLAGS})
target_compile_definitions(luajit PRIVATE ${LUAJIT_C_DEFS} ${LUAJIT_ARCH_DEFS})
set_target_properties(luajit PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "deps")
if(NOT WIN32)
  target_link_libraries(luajit PUBLIC m ${CMAKE_DL_LIBS})
endif()

# --- optional command line interpreter (handy for testing resources) -----------
if(ORANGE_BUILD_TOOLS)
  add_executable(luajit-cli "${LUAJIT_SRC}/luajit.c")
  target_link_libraries(luajit-cli PRIVATE luajit)
  target_compile_options(luajit-cli PRIVATE ${LUAJIT_C_FLAGS})
  target_compile_definitions(luajit-cli PRIVATE ${LUAJIT_C_DEFS})
  set_target_properties(luajit-cli PROPERTIES OUTPUT_NAME luajit RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools" FOLDER "tools")
  if(NOT WIN32)
    # let Lua C modules loaded by the interpreter see the Lua API
    set_target_properties(luajit-cli PROPERTIES ENABLE_EXPORTS ON)
  endif()
endif()
