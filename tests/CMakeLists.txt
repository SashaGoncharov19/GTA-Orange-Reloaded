# Unit tests (platform independent parts only). Run with: ctest --output-on-failure

add_executable(manifest_test manifest_test.cpp)
target_include_directories(manifest_test PRIVATE "${CMAKE_SOURCE_DIR}/Launcher")
set_target_properties(manifest_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
  FOLDER "tests")
add_test(NAME manifest_test COMMAND manifest_test)
