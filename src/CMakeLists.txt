# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src-control/*.* ${CMAKE_SOURCE_DIR}/common/src/*.*)

list(APPEND EXTRA_COMPONENT_DIRS components)

idf_component_register(SRCS ${app_sources})

