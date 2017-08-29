#  Copyright (C) 2017 Sven Willner <sven.willner@gmail.com>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Affero General Public License as published
#  by the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Affero General Public License for more details.
#
#  You should have received a copy of the GNU Affero General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

set(HELPER_MODULES_PATH ${CMAKE_CURRENT_LIST_DIR})


function(set_advanced_cpp_warnings TARGET)
  if(ARGN GREATER 1)
    option(CXX_WARNINGS ON)
  else()
    option(CXX_WARNINGS OFF)
  endif()
  if(CXX_WARNINGS)
    target_compile_options(${TARGET} PRIVATE -Wall -pedantic -Wextra -Wno-reorder)
  endif()
endfunction()


function(set_default_build_type BUILD_TYPE)
  if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.8)
    cmake_policy(SET CMP0069 NEW) # for INTERPROCEDURAL_OPTIMIZATION
  endif()
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE ${BUILD_TYPE} CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
  endif()
endfunction()


function(set_build_type_specifics TARGET)
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.8)
      message(STATUS "Enabling interprocedural optimization")
      set_property(TARGET ${TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
    target_compile_definitions(${TARGET} PUBLIC NDEBUG)
  else()
    target_compile_definitions(${TARGET} PRIVATE DEBUG)
  endif()
endfunction()


function(get_depends_properties RESULT_NAME TARGET PROPERTIES)
  foreach(PROPERTY ${PROPERTIES})
    set(RESULT_${PROPERTY})
  endforeach()
  get_target_property(INTERFACE_LINK_LIBRARIES ${TARGET} INTERFACE_LINK_LIBRARIES)
  if(INTERFACE_LINK_LIBRARIES)
    foreach(INTERFACE_LINK_LIBRARY ${INTERFACE_LINK_LIBRARIES})
      if(TARGET ${INTERFACE_LINK_LIBRARY})
        get_depends_properties(TMP ${INTERFACE_LINK_LIBRARY} "${PROPERTIES}")
        foreach(PROPERTY ${PROPERTIES})
          set(RESULT_${PROPERTY} ${RESULT_${PROPERTY}} ${TMP_${PROPERTY}})
        endforeach()
      endif()
    endforeach()
  endif()
  foreach(PROPERTY ${PROPERTIES})
    get_target_property(TMP ${TARGET} ${PROPERTY})
    if(TMP)
      set(RESULT_${PROPERTY} ${RESULT_${PROPERTY}} ${TMP})
    endif()
    set(${RESULT_NAME}_${PROPERTY} ${RESULT_${PROPERTY}} PARENT_SCOPE)
  endforeach()
endfunction()


function(get_all_include_directories RESULT_NAME TARGET)
  get_depends_properties(RESULT ${TARGET} "INTERFACE_INCLUDE_DIRECTORIES;INTERFACE_SYSTEM_INCLUDE_DIRECTORIES")
  set(RESULT ${RESULT_INTERFACE_INCLUDE_DIRECTORIES} ${RESULT_INTERFACE_SYSTEM_INCLUDE_DIRECTORIES})
  get_target_property(INCLUDE_DIRECTORIES ${TARGET} INCLUDE_DIRECTORIES)
  if(INCLUDE_DIRECTORIES)
    set(RESULT ${RESULT} ${INCLUDE_DIRECTORIES})
  endif()
  if(RESULT)
    list(REMOVE_DUPLICATES RESULT)
  endif()
  set(${RESULT_NAME} ${RESULT} PARENT_SCOPE)
endfunction()


function(get_all_compile_definitions RESULT_NAME TARGET)
  get_depends_properties(RESULT ${TARGET} "INTERFACE_COMPILE_DEFINITIONS")
  set(RESULT ${RESULT_INTERFACE_COMPILE_DEFINITIONS})
  get_target_property(COMPILE_DEFINITIONS ${TARGET} COMPILE_DEFINITIONS)
  if(COMPILE_DEFINITIONS)
    set(RESULT ${RESULT} ${COMPILE_DEFINITIONS})
  endif()
  if(RESULT)
    list(REMOVE_DUPLICATES RESULT)
  endif()
  set(${RESULT_NAME} ${RESULT} PARENT_SCOPE)
endfunction()


function(add_on_source TARGET)
  cmake_parse_arguments(ARGS "" "COMMAND;NAME" "ARGUMENTS" ${ARGN})
  if(ARGS_COMMAND)
    if(NOT ARGS_NAME)
      set(ARGS_NAME ${TARGET}_${ARGS_COMMAND})
    endif()
    find_program(${ARGS_COMMAND}_PATH ${ARGS_COMMAND})
    mark_as_advanced(${ARGS_COMMAND}_PATH)
    if(${ARGS_COMMAND}_PATH)
      set(ARGS)
      set(PER_SOURCEFILE FALSE)

      foreach(ARG ${ARGS_ARGUMENTS})
        if(${ARG} STREQUAL "INCLUDES")
          get_all_include_directories(INCLUDE_DIRECTORIES ${TARGET})
          if(INCLUDE_DIRECTORIES)
            foreach(INCLUDE_DIRECTORY ${INCLUDE_DIRECTORIES})
              set(ARGS ${ARGS} "-I${INCLUDE_DIRECTORY}")
            endforeach()
          endif()
        elseif(${ARG} STREQUAL "DEFINITIONS")
          get_all_compile_definitions(COMPILE_DEFINITIONS ${TARGET})
          if(COMPILE_DEFINITIONS)
            foreach(COMPILE_DEFINITION ${COMPILE_DEFINITIONS})
              set(ARGS ${ARGS} "-D${COMPILE_DEFINITION}")
            endforeach()
          endif()
        elseif(${ARG} STREQUAL "ALL_SOURCEFILES")
          get_target_property(SOURCES ${TARGET} SOURCES)
          set(ARGS ${ARGS} ${SOURCES})
        elseif(${ARG} STREQUAL "SOURCEFILE")
          set(ARGS ${ARGS} ${ARG})
          set(PER_SOURCEFILE TRUE)
        else()
          set(ARGS ${ARGS} ${ARG})
        endif()
      endforeach()

      if(PER_SOURCEFILE)
        get_target_property(SOURCES ${TARGET} SOURCES)
        set(COMMANDS)
        foreach(FILE ${SOURCES})
          set(LOCAL_ARGS)
          foreach(ARG ${ARGS})
            if(${ARG} STREQUAL "SOURCEFILE")
              set(LOCAL_ARGS ${LOCAL_ARGS} ${FILE})
            else()
              set(LOCAL_ARGS ${LOCAL_ARGS} ${ARG})
            endif()
          endforeach()
          file(GLOB FILE ${FILE})
          if(FILE)
            file(RELATIVE_PATH FILE ${CMAKE_CURRENT_SOURCE_DIR} ${FILE})
            add_custom_command(
              OUTPUT ${ARGS_NAME}/${FILE}
              COMMAND ${${ARGS_COMMAND}_PATH} ${LOCAL_ARGS}
              WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
              COMMENT "Running ${ARGS_NAME} on ${FILE}..."
              VERBATIM)
            set_source_files_properties(${ARGS_NAME}/${FILE} PROPERTIES SYMBOLIC TRUE)
            set(COMMANDS ${COMMANDS} ${ARGS_NAME}/${FILE})
          endif()
        endforeach()
        add_custom_target(
          ${ARGS_NAME}
          DEPENDS ${COMMANDS}
          COMMENT "Running ${ARGS_NAME} on ${TARGET}...")
      else()
        add_custom_target(
          ${ARGS_NAME}
          COMMAND ${${ARGS_COMMAND}_PATH} ${ARGS}
          WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
          COMMENT "Running ${ARGS_NAME} on ${TARGET}..."
          VERBATIM)
      endif()
    endif()
  endif()
endfunction()


function(add_cpp_tools TARGET)
  set(CPP_TARGETS)

  add_on_source(${TARGET}
    NAME ${TARGET}_clang_format
    COMMAND clang-format
    ARGUMENTS -i --style=file ALL_SOURCEFILES)
  if(TARGET ${TARGET}_clang_format)
    set(CPP_TARGETS ${CPP_TARGETS} ${TARGET}_clang_format)
  endif()

  add_on_source(${TARGET}
    NAME ${TARGET}_clang_tidy
    COMMAND clang-tidy
    ARGUMENTS -quiet SOURCEFILE -- -std=c++11 INCLUDES DEFINITIONS)
  if(TARGET ${TARGET}_clang_tidy)
    set(CPP_TARGETS ${CPP_TARGETS} ${TARGET}_clang_tidy)
  endif()

  add_on_source(${TARGET}
    NAME ${TARGET}_clang_tidy_fix
    COMMAND clang-tidy
    ARGUMENTS -quiet -fix -format-style=file SOURCEFILE -- -std=c++11 INCLUDES DEFINITIONS)

  add_on_source(${TARGET}
    NAME ${TARGET}_cppcheck
    COMMAND cppcheck
    ARGUMENTS INCLUDES DEFINITIONS --quiet --template=gcc --enable=all ALL_SOURCEFILES)
  if(TARGET ${TARGET}_cppcheck)
    set(CPP_TARGETS ${CPP_TARGETS} ${TARGET}_cppcheck)
  endif()

  add_on_source(${TARGET}
    NAME ${TARGET}_cppclean
    COMMAND cppclean
    ARGUMENTS INCLUDES ALL_SOURCEFILES)
  if(TARGET ${TARGET}_cppclean)
    set(CPP_TARGETS ${CPP_TARGETS} ${TARGET}_cppclean)
  endif()

  add_on_source(${TARGET}
    NAME ${TARGET}_iwyu
    COMMAND iwyu
    ARGUMENTS -std=c++11 -I/usr/include/clang/3.8/include INCLUDES DEFINITIONS SOURCEFILE)
  if(TARGET ${TARGET}_iwyu)
    set(CPP_TARGETS ${CPP_TARGETS} ${TARGET}_iwyu)
  endif()

  if(CPP_TARGETS)
    add_custom_target(${TARGET}_cpp_tools
      DEPENDS ${CPP_TARGETS})
  endif()
endfunction()


function(add_git_version TARGET)
  cmake_parse_arguments(ARGS "WITH_DIFF" "FALLBACK_VERSION;DPREFIX;DIFF_VAR" "" ${ARGN})
  if(NOT ARGS_FALLBACK_VERSION)
    set(ARGS_FALLBACK_VERSION "UNKNOWN")
  endif()
  if(NOT ARGS_DPREFIX)
    string(MAKE_C_IDENTIFIER "${TARGET}" ARGS_DPREFIX)
    string(TOUPPER ${ARGS_DPREFIX} ARGS_DPREFIX)
  endif()
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/git_version/version.h
    "#ifndef ${ARGS_DPREFIX}_VERSION_H\n"
    "#define ${ARGS_DPREFIX}_VERSION_H\n"
    "#define ${ARGS_DPREFIX}_VERSION \"${ARGS_FALLBACK_VERSION}\"\n"
    "#endif")
  if(NOT ARGS_DIFF_VAR)
    string(TOLOWER ${ARGS_DPREFIX}_git_diff ARGS_DIFF_VAR)
  endif()
  if(EXISTS "${CMAKE_SOURCE_DIR}/.git" AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
    find_program(HAVE_GIT git)
    mark_as_advanced(HAVE_GIT)
    if(HAVE_GIT)

      #add_custom_target(${TARGET}_version ALL
      #  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/git_version/version.h)

      #add_custom_command(
      #  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/git_version/version.h
      #  COMMAND ${CMAKE_COMMAND}
      #  -DARGS_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
      #  -DARGS_DIFF_VAR=${ARGS_DIFF_VAR}
      #  -DARGS_DPREFIX=${ARGS_DPREFIX}
      #  -DARGS_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      #  -DARGS_WITH_DIFF=${ARGS_WITH_DIFF}
      #  -P ${HELPER_MODULES_PATH}/git_version.cmake)

      add_custom_target(${TARGET}_version ALL
        COMMAND ${CMAKE_COMMAND}
        -DARGS_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -DARGS_DIFF_VAR=${ARGS_DIFF_VAR}
        -DARGS_DPREFIX=${ARGS_DPREFIX}
        -DARGS_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DARGS_WITH_DIFF=${ARGS_WITH_DIFF}
        -P ${HELPER_MODULES_PATH}/git_version.cmake)

      set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/git_version/version.h
        PROPERTIES GENERATED TRUE
        HEADER_FILE_ONLY TRUE)

      target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/git_version)

      add_dependencies(${TARGET} ${TARGET}_version)

      if(ARGS_WITH_DIFF)
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/git_version/diff.cpp
          "const char* ${ARGS_DIFF_VAR} = \"UNKNOWN\";\n")
        set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/git_version/diff.cpp
          PROPERTIES GENERATED TRUE)
      endif()

    endif()
  endif()
  if(ARGS_WITH_DIFF)
    if(NOT HAVE_GIT)
      file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/git_version/diff.cpp
        "const char* ${ARGS_DIFF_VAR} = \"\";\n")
    endif()
    target_sources(${TARGET} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/git_version/diff.cpp)
  endif()
endfunction()
