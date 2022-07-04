# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022 Intel Corporation

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
      dpcpp
    HINTS
      ${_sycl_search_dirs}
    PATH_SUFFIXES
      bin
  )
endif()

FIND_LIBRARY(SYCL_LIBRARY
  NAMES
    sycl
  HINTS
    ${_sycl_search_dirs}
  PATH_SUFFIXES
    lib64 lib
)

FIND_PATH(SYCL_INCLUDE_DIR
  NAMES
    CL/sycl.hpp
  HINTS
    ${_sycl_search_dirs}
  PATH_SUFFIXES
    include
    include/sycl
)

INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SYCL DEFAULT_MSG SYCL_LIBRARY SYCL_INCLUDE_DIR)

IF(SYCL_FOUND)
  get_filename_component(_SYCL_INCLUDE_PARENT_DIR ${SYCL_INCLUDE_DIR} DIRECTORY)
  SET(SYCL_INCLUDE_DIR ${SYCL_INCLUDE_DIR} ${_SYCL_INCLUDE_PARENT_DIR})
ELSE()
  SET(SYCL_SYCL_FOUND FALSE)
ENDIF()

MARK_AS_ADVANCED(
  _SYCL_INCLUDE_PARENT_DIR
)
