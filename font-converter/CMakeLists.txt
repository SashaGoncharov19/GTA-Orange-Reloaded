# font-converter: ImGui's binary_to_compressed_c helper used to embed fonts.
add_executable(font-converter font-converter.cpp)
set_target_properties(font-converter PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools"
  FOLDER "tools")
