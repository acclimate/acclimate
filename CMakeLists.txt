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

cmake_minimum_required(VERSION 2.8)

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

target_include_directories(acclimate PRIVATE include)
target_compile_options(acclimate PRIVATE -std=c++11)

add_doxygen_documentation(doc doc)
set_ccache_use()
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

set(ACCLIMATE_OPTIONS "")
set(ACCLIMATE_VARIANTS "")
function(acclimate_include_option OPTIONS_NAME NAME DESC STATE)
  option(ACCLIMATE_${NAME} ${DESC} ${STATE})
  if(ACCLIMATE_${NAME})
    set(ACCLIMATE_${OPTIONS_NAME} ${ACCLIMATE_${OPTIONS_NAME}} ${NAME} PARENT_SCOPE)
    target_compile_definitions(acclimate PRIVATE ${NAME})
  endif()
endfunction()

acclimate_include_option(OPTIONS BASED_ON_INT "automatic rounding based on internal integers" ON)
mark_as_advanced(ACCLIMATE_BASED_ON_INT)
acclimate_include_option(OPTIONS BANKERS_ROUNDING "bankers rounding" ON)
acclimate_include_option(OPTIONS FLOATING_POINT_EXCEPTIONS "" OFF)
acclimate_include_option(OPTIONS FATAL_FLOATING_POINT_EXCEPTIONS "" OFF)
acclimate_include_option(OPTIONS STRICT_MIN_DERIVATIVE "" OFF)
acclimate_include_option(OPTIONS CLEANUP_INFO "show additional information about the cleanup (only for Debug)" OFF)
acclimate_include_option(OPTIONS OPTIMIZATION_WARNINGS "show warnings for optimization (only for Debug)" OFF)
acclimate_include_option(OPTIONS OPTIMIZATION_PROBLEMS_FATAL "make optimization problems fatal (only for Debug)" OFF)
acclimate_include_option(OPTIONS ENABLE_DMTCP "enable dmtcp checkpointing" OFF)
acclimate_include_option(OPTIONS USE_TQDM "enable progress output using tqdm.cpp" OFF)

if(ACCLIMATE_USE_TQDM)
  include(ExternalProject)
  ExternalProject_Add(tqdm.cpp
    GIT_REPOSITORY https://github.com/swillner/tqdm.cpp
    GIT_TAG "bd7707e"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    )
  ExternalProject_Get_Property(tqdm.cpp SOURCE_DIR)
  target_include_directories(acclimate PRIVATE ${SOURCE_DIR}/include)
  message(STATUS "Including tqdm.cpp from git")
endif()

if(ACCLIMATE_ENABLE_DMTCP)
  set_property(TARGET acclimate PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()
string(REPLACE ";" ", " ACCLIMATE_OPTIONS "${ACCLIMATE_OPTIONS}")
target_compile_definitions(acclimate PRIVATE ACCLIMATE_OPTIONS="${ACCLIMATE_OPTIONS}")

acclimate_include_option(VARIANTS VARIANT_BASIC "include acclimate version 'basic'" ON)
acclimate_include_option(VARIANTS VARIANT_DEMAND "include acclimate version 'demand'" ON)
acclimate_include_option(VARIANTS VARIANT_PRICES "include acclimate version 'prices'" ON)

string(REPLACE ";" ", " ACCLIMATE_VARIANTS "${ACCLIMATE_VARIANTS}")
target_compile_definitions(acclimate PRIVATE ACCLIMATE_VARIANTS="${ACCLIMATE_VARIANTS}")

# include nlopt
include_nlopt(acclimate OFF "34fd7ce")

# include NetCDF C library
find_package(NETCDF REQUIRED)
message(STATUS "NetCDF include directory: ${NETCDF_INCLUDE_DIR}")
message(STATUS "NetCDF library: ${NETCDF_LIBRARY}")
target_link_libraries(acclimate PRIVATE netcdf)

# include NetCDF C++ library
find_package(NETCDF_CPP4 REQUIRED)
message(STATUS "NetCDF_c++4 include directory: ${NETCDF_CPP4_INCLUDE_DIR}")
message(STATUS "NetCDF_c++4 library: ${NETCDF_CPP4_LIBRARY}")
target_link_libraries(acclimate PRIVATE netcdf_c++4)

include(CheckStructHasMember)
set(CMAKE_REQUIRED_INCLUDES "${NETCDF_CPP4_INCLUDE_DIR}" "${NETCDF_INCLUDE_DIR}")
set(CMAKE_REQUIRED_LIBRARIES "${NETCDF_CPP4_LIBRARY}" "${NETCDF_LIBRARY}" "${HDF5_LIBRARY}")
check_struct_has_member("netCDF::NcFile" "sync()" "ncFile.h" HAVE_NEW_NETCDF_CPP LANGUAGE CXX)
if(NOT HAVE_NEW_NETCDF_CPP)
  message(FATAL_ERROR "NetCDF-C++ does not include sync procedure, need a newer library version")
endif()

include(lib/settingsnode/settingsnode.cmake)
include_settingsnode(acclimate)
include_yaml_cpp(acclimate ON "yaml-cpp-0.6.2")

include(lib/libmrio/libmrio.cmake)
target_link_libraries(acclimate PRIVATE libmrio)
mark_as_advanced(LIBMRIO_WITH_NETCDF)

add_cpp_tools(acclimate)
