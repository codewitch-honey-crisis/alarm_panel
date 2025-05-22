cmake_minimum_required(VERSION 3.24)

project(win32_www VERSION 1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_PREFIX "")

execute_process(
  COMMAND rebuild_react.cmd
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_FILE log.txt        # capture stdout in a file
  OUTPUT_VARIABLE logVar      # Capture stdout in a variable
  ERROR_VARIABLE errorVar     # Capture stderr in a variable
  RESULT_VARIABLE resultVar   # Capture exit code or error string 
  COMMAND_ERROR_IS_FATAL ANY  # stop CMake if failed
)

execute_process(
  COMMAND rebuild_clasp.cmd
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_FILE log.txt        # capture stdout in a file
  OUTPUT_VARIABLE logVar      # Capture stdout in a variable
  ERROR_VARIABLE errorVar     # Capture stderr in a variable
  RESULT_VARIABLE resultVar   # Capture exit code or error string 
  COMMAND_ERROR_IS_FATAL ANY  # stop CMake if failed
)

add_executable(win32_www src/main.cpp)
target_link_libraries(win32_www ws2_32)
target_include_directories(win32_www PUBLIC
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
  )
