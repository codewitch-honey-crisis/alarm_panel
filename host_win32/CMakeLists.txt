cmake_minimum_required(VERSION 3.24)

project(host_win32 VERSION 1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_compile_definitions(host_win32 PRIVATE LOCAL_PC)

add_compile_definitions(host_win32 PRIVATE _UNICODE UNICODE)

include(FetchContent)
FetchContent_Declare(htcw_uix
    GIT_REPOSITORY https://github.com/codewitch-honey-crisis/uix.git
    GIT_TAG 1.6.5
    GIT_SHALLOW ON
    FIND_PACKAGE_ARGS 1.6.5
)
FetchContent_MakeAvailable(htcw_uix)

add_executable(host_win32 WIN32 src/main.cpp src/display.cpp src/base64.c src/sha1.c src/serial.c src/spi.c src/task.c src/power.c src/network.c src/httpd.c ../common/src/alarm.c ../common/src/ws_server.c ../common/src/ui.cpp ../common/src/control.cpp)

add_custom_command(
  TARGET host_win32
  PRE_BUILD
  COMMAND rebuild_react.cmd
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

add_custom_command(
  TARGET host_win32
  PRE_BUILD
  COMMAND rebuild_clasp.cmd
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

target_link_libraries(host_win32 ws2_32 kernel32 user32 gdi32 htcw_bits htcw_io htcw_ml htcw_data htcw_gfx htcw_uix)
target_include_directories(host_win32 PUBLIC
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/../common/include"
    "${PROJECT_SOURCE_DIR}/../common/src"
  )
