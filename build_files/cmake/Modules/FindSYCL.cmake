# SPDX-FileCopyrightText: 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find SYCL library
# Find the native SYCL header and libraries needed by oneAPI implementation
# This module defines
#  SYCL_COMPILER, compiler which will be used for compilation of SYCL code
#  SYCL_LIBRARY, libraries to link against in order to use SYCL.
#  SYCL_INCLUDE_DIR, directories where SYCL headers can be found
#  SYCL_ROOT_DIR, The base directory to search for SYCL files.
#                 This can also be an environment variable.
#  SYCL_FOUND, If false, then don't try to use SYCL.

IF(NOT SYCL_ROOT_DIR AND NOT $ENV{SYCL_ROOT_DIR} STREQUAL "")
  SET(SYCL_ROOT_DIR $ENV{SYCL_ROOT_DIR})
ENDIF()

SET(_sycl_search_dirs
  ${SYCL_ROOT_DIR}
  /usr/lib
  /usr/local/lib
  /opt/intel/oneapi/compiler/latest/linux/
  C:/Program\ Files\ \(x86\)/Intel/oneAPI/compiler/latest/windows
)

# Find DPC++ compiler.
# Since the compiler name is possibly conflicting with the system-wide
# CLang start with looking for either dpcpp or clang binary in the given
# list of search paths only. If that fails, try to look for a system-wide
# dpcpp binary.
FIND_PROGRAM(SYCL_COMPILER
  NAMES
    icpx
    dpcpp
    clang++
  HINTS
    ${_sycl_search_dirs}
  PATH_SUFFIXES
    bin
  NO_CMAKE_FIND_ROOT_PATH
  NAMES_PER_DIR
)

# NOTE: No clang++ here so that we do not pick up a system-wide CLang
# compiler.
if(NOT SYCL_COMPILER)
  FIND_PROGRAM(SYCL_COMPILER
    NAMES
      icpx
      dpcpp
    HINTS
      ${_sycl_search_dirs}
    PATH_SUFFIXES
      bin
  )
endif()

FIND_LIBRARY(SYCL_LIBRARY
  NAMES
    sycl7
    sycl6
    sycl
  HINTS
    ${_sycl_search_dirs}
  PATH_SUFFIXES
    lib64 lib
)

if(WIN32)
  FIND_LIBRARY(SYCL_LIBRARY_DEBUG
    NAMES
      sycl7d
      sycl6d
      sycld
    HINTS
      ${_sycl_search_dirs}
    PATH_SUFFIXES
      lib64 lib
  )
endif()

FIND_PATH(SYCL_INCLUDE_DIR
  NAMES
    sycl/sycl.hpp
  HINTS
    ${_sycl_search_dirs}
  PATH_SUFFIXES
    include
)

IF(EXISTS "${SYCL_INCLUDE_DIR}/sycl/version.hpp")
  FILE(STRINGS "${SYCL_INCLUDE_DIR}/sycl/version.hpp" _libsycl_major_version REGEX "^#define __LIBSYCL_MAJOR_VERSION[ \t].*$")
  STRING(REGEX MATCHALL "[0-9]+" _libsycl_major_version ${_libsycl_major_version})
  FILE(STRINGS "${SYCL_INCLUDE_DIR}/sycl/version.hpp" _libsycl_minor_version REGEX "^#define __LIBSYCL_MINOR_VERSION[ \t].*$")
  STRING(REGEX MATCHALL "[0-9]+" _libsycl_minor_version ${_libsycl_minor_version})
  FILE(STRINGS "${SYCL_INCLUDE_DIR}/sycl/version.hpp" _libsycl_patch_version REGEX "^#define __LIBSYCL_PATCH_VERSION[ \t].*$")
  STRING(REGEX MATCHALL "[0-9]+" _libsycl_patch_version ${_libsycl_patch_version})

  SET(SYCL_VERSION "${_libsycl_major_version}.${_libsycl_minor_version}.${_libsycl_patch_version}")
ENDIF()

INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SYCL
  REQUIRED_VARS SYCL_LIBRARY SYCL_INCLUDE_DIR
  VERSION_VAR SYCL_VERSION
)

IF(SYCL_FOUND)
  SET(SYCL_INCLUDE_DIR ${SYCL_INCLUDE_DIR} ${SYCL_INCLUDE_DIR}/sycl)
  IF(WIN32 AND SYCL_LIBRARY_DEBUG)
    SET(SYCL_LIBRARIES optimized ${SYCL_LIBRARY} debug ${SYCL_LIBRARY_DEBUG})
  ELSE()
    SET(SYCL_LIBRARIES ${SYCL_LIBRARY})
  ENDIF()
ELSE()
  SET(SYCL_SYCL_FOUND FALSE)
ENDIF()

MARK_AS_ADVANCED(
  SYCL_COMPILER
  SYCL_INCLUDE_DIR
  SYCL_LIBRARY
)
