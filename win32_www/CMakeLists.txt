cmake_minimum_required(VERSION 3.24)

project(win32_www VERSION 1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_executable(win32_www src/main.cpp src/base64.c src/sha1.c src/ws_server.c)

add_custom_command(
  TARGET win32_www
  PRE_BUILD
  COMMAND rebuild_react.cmd
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

add_custom_command(
  TARGET win32_www
  PRE_BUILD
  COMMAND rebuild_clasp.cmd
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

target_link_libraries(win32_www ws2_32)
target_include_directories(win32_www PUBLIC
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
  )
