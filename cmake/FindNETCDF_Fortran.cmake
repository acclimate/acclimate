if(NETCDF_Fortran_INCLUDE_DIR AND NETCDF_Fortran_LIBRARY)
  set(NETCDF_Fortran_FIND_QUIETLY TRUE)
endif()

find_path(NETCDF_Fortran_INCLUDE_DIR NAMES netcdf.mod)
find_library(NETCDF_Fortran_LIBRARY NAMES netcdff)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NETCDF_Fortran DEFAULT_MSG NETCDF_Fortran_INCLUDE_DIR NETCDF_Fortran_LIBRARY)

mark_as_advanced(NETCDF_Fortran_INCLUDE_DIR NETCDF_Fortran_LIBRARY)

set(NETCDF_Fortran_LIBRARIES ${NETCDF_Fortran_LIBRARY})
set(NETCDF_Fortran_INCLUDE_DIRS ${NETCDF_Fortran_INCLUDE_DIR})

if(NETCDF_Fortran_FOUND AND NOT TARGET netcdff)
  add_library(netcdff UNKNOWN IMPORTED)
  set_target_properties(netcdff PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${NETCDF_Fortran_INCLUDE_DIRS}")
  set_target_properties(netcdff PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "Fortran")
  set_target_properties(netcdff PROPERTIES IMPORTED_LOCATION "${NETCDF_Fortran_LIBRARIES}")
  find_package(NETCDF REQUIRED)
  set_target_properties(netcdff PROPERTIES INTERFACE_LINK_LIBRARIES "${NETCDF_LIBRARIES}")
endif()
