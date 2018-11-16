if(TARGET libmrio)
  return()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/cmake)

add_library(libmrio STATIC ${CMAKE_CURRENT_LIST_DIR}/src/Disaggregation.cpp ${CMAKE_CURRENT_LIST_DIR}/src/MRIOIndexSet.cpp ${CMAKE_CURRENT_LIST_DIR}/src/MRIOTable.cpp)
target_include_directories(libmrio PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${CMAKE_CURRENT_LIST_DIR}/lib/cpp-library)
target_compile_options(libmrio PRIVATE "-std=c++11")

option(LIBMRIO_WITH_NETCDF "NetCDF" ON)
if(LIBMRIO_WITH_NETCDF)
  include_netcdf_cxx4(libmrio ON v4.3.0)
  target_compile_definitions(libmrio PUBLIC LIBMRIO_WITH_NETCDF)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.8)
    set_property(TARGET libmrio PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
  target_compile_definitions(libmrio PUBLIC NDEBUG)
else()
  target_compile_definitions(libmrio PRIVATE DEBUG)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/lib/settingsnode/settingsnode.cmake)
include_settingsnode(libmrio)
