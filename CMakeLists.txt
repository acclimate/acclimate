cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
include(cmake/helpers.cmake)

set_default_build_type(Release)

project(settingsnode)
add_executable(test test.cpp)

target_include_directories(test PRIVATE include)
target_compile_options(test PRIVATE -std=c++11)
set_advanced_cpp_warnings(test)
set_build_type_specifics(test)

include(settingsnode.cmake)
target_link_libraries(test settingsnode)

add_cpp_tools(test)
