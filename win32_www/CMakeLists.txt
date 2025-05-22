cmake_minimum_required(VERSION 3.24)

project(win32_www VERSION 1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_PREFIX "")

#add_compile_definitions(win32_www PUBLIC FOO)

add_executable(win32_www src/main.cpp)
target_link_libraries(win32_www ws2_32)
target_include_directories(win32_www PUBLIC
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
  )
