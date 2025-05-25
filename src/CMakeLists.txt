# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src-esp-idf/*.* ${CMAKE_SOURCE_DIR}/common/src/*.*)

idf_component_register(SRCS ${app_sources})
