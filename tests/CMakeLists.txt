# Unit tests (platform independent parts only). Run with: ctest --output-on-failure

if(EXISTS "${CMAKE_SOURCE_DIR}/Launcher/UpdateManifest.h")
  add_executable(manifest_test manifest_test.cpp)
  target_include_directories(manifest_test PRIVATE "${CMAKE_SOURCE_DIR}/Launcher")
  set_target_properties(manifest_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
    FOLDER "tests")
  add_test(NAME manifest_test COMMAND manifest_test)
endif()

# offsets.ini reader / pattern scanner used by orange-core (shared/OffsetSpec.h)
add_executable(offsets_test offsets_test.cpp)
target_include_directories(offsets_test PRIVATE "${ORANGE_SHARED_DIR}")
set_target_properties(offsets_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
  FOLDER "tests")
add_test(NAME offsets_test COMMAND offsets_test)

# natives crossmap reader + the generated reference table (shared/NativeCrossmap.h)
add_executable(natives_crossmap_test natives_crossmap_test.cpp)
target_include_directories(natives_crossmap_test PRIVATE "${ORANGE_SHARED_DIR}" "${CMAKE_SOURCE_DIR}/orange-core/Core")
set_target_properties(natives_crossmap_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
  FOLDER "tests")
add_test(NAME natives_crossmap_test COMMAND natives_crossmap_test)
