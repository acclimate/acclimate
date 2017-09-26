#Copyright(C) 2014 - 2017 Sven Willner < sven.willner @pik - potsdam.de >
#Christian Otto < christian.otto @pik - potsdam.de >
#
#This file is part of Acclimate.
#
#Acclimate is free software : you can redistribute it and / or modify
#it under the terms of the GNU Affero General Public License as
#published by the Free Software Foundation, either version 3 of
#the License, or (at your option) any later version.
#
#Acclimate is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
#GNU Affero General Public License for more details.
#
#You should have received a copy of the GNU Affero General Public License
#along with Acclimate.If not, see < http:  // www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
include(cmake/helpers.cmake)

set_default_build_type(Release)

project(acclimate)

file(GLOB ACCLIMATE_SOURCES src/**/*.cpp)
set(ACCLIMATE_SOURCES ${ACCLIMATE_SOURCES} src/acclimate.cpp)

option(ACCLIMATE_SHARED_LIBRARY "" OFF)
if(ACCLIMATE_SHARED_LIBRARY)
  add_library(acclimate ${ACCLIMATE_SOURCES} src/external.cpp)
else()
  add_executable(acclimate ${ACCLIMATE_SOURCES} src/main.cpp)
endif()

target_include_directories(acclimate PRIVATE include)
target_compile_options(acclimate PRIVATE -std=c++11)

set_advanced_cpp_warnings(acclimate)
set_build_type_specifics(acclimate)
add_git_version(acclimate
  FALLBACK_VERSION 3.0.0
  WITH_DIFF)

option(ACCLIMATE_PARALLELIZATION "" ON)
if(ACCLIMATE_PARALLELIZATION)
  find_package(OpenMP REQUIRED)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

option(ACCLIMATE_BASED_ON_INT "automatic rounding based on internal integers" ON)
mark_as_advanced(ACCLIMATE_BASED_ON_INT)
option(ACCLIMATE_BANKERS_ROUNDING "bankers rounding" ON)
option(ACCLIMATE_SUB_PARALLELIZATION "" OFF)
option(ACCLIMATE_FLOATING_POINT_EXCEPTIONS "" OFF)
option(ACCLIMATE_FATAL_FLOATING_POINT_EXCEPTIONS "" OFF)
option(ACCLIMATE_TRANSPORT "" OFF)
mark_as_advanced(ACCLIMATE_TRANSPORT)
option(ACCLIMATE_STRICT_MIN_DERIVATIVE "" OFF)
option(ACCLIMATE_CLEANUP_INFO "show additional information about the cleanup (only for Debug)" OFF)
option(ACCLIMATE_OPTIMIZATION_WARNINGS "show warnings for optimization (only for Debug)" OFF)
option(ACCLIMATE_OPTIMIZATION_PROBLEMS_FATAL "make optimization problems fatal (only for Debug)" OFF)

set(ACCLIMATE_OPTIONS "")
function(acclimate_include_option OPT NAME)
  if(${OPT})
    set(ACCLIMATE_OPTIONS ${ACCLIMATE_OPTIONS} ${NAME} PARENT_SCOPE)
  endif()
endfunction()
acclimate_include_option(ACCLIMATE_BASED_ON_INT "BASED_ON_INT")
acclimate_include_option(ACCLIMATE_BANKERS_ROUNDING "BANKERS_ROUNDING")
acclimate_include_option(ACCLIMATE_SUB_PARALLELIZATION "SUB_PARALLELIZATION")
acclimate_include_option(ACCLIMATE_FLOATING_POINT_EXCEPTIONS "FLOATING_POINT_EXCEPTIONS")
acclimate_include_option(ACCLIMATE_FATAL_FLOATING_POINT_EXCEPTIONS "FATAL_FLOATING_POINT_EXCEPTIONS")
acclimate_include_option(ACCLIMATE_TRANSPORT "TRANSPORT")
acclimate_include_option(ACCLIMATE_STRICT_MIN_DERIVATIVE "STRICT_MIN_DERIVATIVE")
acclimate_include_option(ACCLIMATE_CLEANUP_INFO "CLEANUP_INFO")
acclimate_include_option(ACCLIMATE_OPTIMIZATION_WARNINGS "OPTIMIZATION_WARNINGS")
acclimate_include_option(ACCLIMATE_OPTIMIZATION_PROBLEMS_FATAL "OPTIMIZATION_PROBLEMS_FATAL")

target_compile_definitions(acclimate PRIVATE ${ACCLIMATE_OPTIONS})
string(REPLACE ";" ", " ACCLIMATE_OPTIONS "${ACCLIMATE_OPTIONS}")
target_compile_definitions(acclimate PRIVATE ACCLIMATE_OPTIONS="${ACCLIMATE_OPTIONS}")

#include nlopt
find_package(NLOPT REQUIRED)
message(STATUS "NLopt include directory: ${NLOPT_INCLUDE_DIR}")
message(STATUS "NLopt library: ${NLOPT_LIBRARY}")
target_link_libraries(acclimate nlopt)

#include NetCDF C library
find_package(NETCDF REQUIRED)
message(STATUS "NetCDF include directory: ${NETCDF_INCLUDE_DIR}")
message(STATUS "NetCDF library: ${NETCDF_LIBRARY}")
target_link_libraries(acclimate netcdf)

#include NetCDF C++ library
find_package(NETCDF_CPP4 REQUIRED)
message(STATUS "NetCDF_c++4 include directory: ${NETCDF_CPP4_INCLUDE_DIR}")
message(STATUS "NetCDF_c++4 library: ${NETCDF_CPP4_LIBRARY}")
target_link_libraries(acclimate netcdf_c++4)

include(CheckCXXSourceCompiles)
set(NETCDF_CHECK_CODE "
#include <ncFile.h>
int main() { netCDF::NcFile file; file.sync(); file.close(); return 0; }
")
set(CMAKE_REQUIRED_LIBRARIES "${NETCDF_CPP4_LIBRARY}" "${NETCDF_LIBRARY}")
set(CMAKE_REQUIRED_INCLUDES "${NETCDF_CPP4_INCLUDE_DIR}" "${NETCDF_INCLUDE_DIR}")
check_cxx_source_compiles("${NETCDF_CHECK_CODE}" HAVE_NEW_NETCDF_CPP)
if(HAVE_NEW_NETCDF_CPP)
  target_compile_definitions(acclimate PRIVATE ACCLIMATE_HAVE_NEW_NETCDF_CPP)
endif()

include(lib/libmrio/libmrio.cmake)
target_link_libraries(acclimate libmrio)
mark_as_advanced(LIBMRIO_WITH_NETCDF)

include(lib/settingsnode/settingsnode.cmake)
target_link_libraries(acclimate settingsnode)

add_cpp_tools(acclimate)
