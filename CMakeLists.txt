# Copyright (C) 2014-2017 Sven Willner <sven.willner@pik-potsdam.de>
#                         Christian Otto <christian.otto@pik-potsdam.de>
#
# This file is part of Acclimate.
#
# Acclimate is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# Acclimate is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with Acclimate.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
include(cmake/helpers.cmake)
include(cmake/libraries.cmake)

set_default_build_type(Release)

project(acclimate)

file(GLOB ACCLIMATE_SOURCES src/**/*.cpp)
set(ACCLIMATE_SOURCES ${ACCLIMATE_SOURCES} src/acclimate.cpp src/info.cpp src/run.cpp)

option(ACCLIMATE_SHARED_LIBRARY "" OFF)
if(ACCLIMATE_SHARED_LIBRARY)
  add_library(acclimate ${ACCLIMATE_SOURCES} src/external.cpp)
else()
  add_executable(acclimate ${ACCLIMATE_SOURCES} src/main.cpp)
endif()

target_include_directories(acclimate PRIVATE include lib/cpp-library)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel"
)# icc 19 does not yet fully support c++17, but we only need features it does support
  target_compile_options(acclimate PUBLIC -std=c++17)
else()
  target_compile_features(acclimate PUBLIC cxx_std_17)
endif()

add_system_paths()
add_doxygen_documentation(doc doc)
set_advanced_cpp_warnings(acclimate)
set_build_type_specifics(acclimate)
add_git_version(acclimate WITH_DIFF)

option(ACCLIMATE_PARALLELIZATION "" ON)
if(ACCLIMATE_PARALLELIZATION)
  find_package(OpenMP REQUIRED)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

set(ACCLIMATE_OPTIONS_FILE "")
set(ACCLIMATE_OPTIONS "")
function(acclimate_include_option NAME DESC STATE)
  option(ACCLIMATE_${NAME} ${DESC} ${STATE})
  if(ACCLIMATE_${NAME})
    set(VALUE "true")
  else()
    set(VALUE "false")
  endif()
  set(ACCLIMATE_OPTIONS
      ${ACCLIMATE_OPTIONS} "Option{\"${NAME}\", ${VALUE}}"
      PARENT_SCOPE
  )
  set(ACCLIMATE_OPTIONS_FILE
      "${ACCLIMATE_OPTIONS_FILE}static constexpr bool ${NAME} = ${VALUE};\n"
      PARENT_SCOPE
  )
endfunction()

acclimate_include_option(BASED_ON_INT "automatic rounding based on internal integers" ON)
mark_as_advanced(ACCLIMATE_BASED_ON_INT)
acclimate_include_option(BANKERS_ROUNDING "bankers rounding" ON)
acclimate_include_option(FLOATING_POINT_EXCEPTIONS "" OFF)
acclimate_include_option(FATAL_FLOATING_POINT_EXCEPTIONS "" OFF)
acclimate_include_option(STRICT_MIN_DERIVATIVE "" OFF)
acclimate_include_option(CLEANUP_INFO "show additional information about the cleanup (only for Debug)" OFF)
acclimate_include_option(OPTIMIZATION_WARNINGS "show warnings for optimization (only for Debug)" OFF)
acclimate_include_option(OPTIMIZATION_PROBLEMS_FATAL "make optimization problems fatal (only for Debug)" OFF)
acclimate_include_option(ENABLE_DMTCP "enable dmtcp checkpointing" OFF)
acclimate_include_option(USE_MIN_PASSAGE_IN_EXPECTATION "enable minimum passage usage in expectation" ON)

if(ACCLIMATE_ENABLE_DMTCP)
  set_property(TARGET acclimate PROPERTY POSITION_INDEPENDENT_CODE ON)
  target_compile_definitions(acclimate PRIVATE ENABLE_DMTCP)
  if(ACCLIMATE_PARALLELIZATION)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
      set_property(
        TARGET acclimate
        APPEND_STRING
        PROPERTY LINK_FLAGS " -qopenmp-link=static"
      )
    endif()
  endif()
endif()

list(LENGTH ACCLIMATE_OPTIONS ACCLIMATE_OPTIONS_LENGTH)
file(
  WRITE ${CMAKE_CURRENT_BINARY_DIR}/include/options.h
  "\
#ifndef ACCLIMATE_OPTIONS_H
#define ACCLIMATE_OPTIONS_H
#include <array>

namespace acclimate::options {

#ifdef DEBUG
constexpr bool DEBUGGING = true;
#else
constexpr bool DEBUGGING = false;
#endif
#ifdef _OPENMP
constexpr bool PARALLELIZATION = true;
#else
constexpr bool PARALLELIZATION = false;
#endif

${ACCLIMATE_OPTIONS_FILE}
struct Option {
    const char* name;
    bool value;
};
extern const std::array<Option, ${ACCLIMATE_OPTIONS_LENGTH}> options;

}  // namespace acclimate::options

#endif
"
)
target_include_directories(acclimate PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)

string(REPLACE ";" ",\n    " ACCLIMATE_OPTIONS "${ACCLIMATE_OPTIONS}")
file(
  WRITE ${CMAKE_CURRENT_BINARY_DIR}/src/options.cpp
  "\
#include \"options.h\"

namespace acclimate::options {

const std::array<Option, ${ACCLIMATE_OPTIONS_LENGTH}> options = {
    ${ACCLIMATE_OPTIONS}
};

}  // namespace acclimate::options
"
)
target_sources(acclimate PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/src/options.cpp)

include_nlopt(acclimate ON "34fd7ce")

include_netcdf_cxx4(acclimate ON v4.3.0)

include(lib/libmrio/libmrio.cmake)
target_link_libraries(acclimate PRIVATE libmrio)
mark_as_advanced(LIBMRIO_WITH_NETCDF)

include(lib/settingsnode/settingsnode.cmake)
include_settingsnode(acclimate)
include_yaml_cpp(acclimate ON "yaml-cpp-0.6.2")

add_cpp_tools(acclimate STD c++17)
