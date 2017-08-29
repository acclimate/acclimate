cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
include(cmake/helpers.cmake)

set_default_build_type(Release)

project(mrio_disaggregate)

add_executable(mrio_disaggregate src/main.cpp)

target_include_directories(mrio_disaggregate PRIVATE include)
target_compile_options(mrio_disaggregate PRIVATE "-std=c++11")

set_advanced_cpp_warnings(mrio_disaggregate)
set_build_type_specifics(mrio_disaggregate)
add_git_version(mrio_disaggregate
  FALLBACK_VERSION 1.0.0)

include(libmrio.cmake)
set_advanced_cpp_warnings(libmrio)
target_link_libraries(mrio_disaggregate libmrio)

include(lib/settingsnode/settingsnode.cmake)
target_link_libraries(mrio_disaggregate settingsnode)

add_cpp_tools(mrio_disaggregate)
add_cpp_tools(libmrio)
