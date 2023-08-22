# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# This module defines
#  LibEpoxy_INCLUDE_DIRS, where to find epoxy/gl.h
#  LibEpoxy_LIBRARY, where to find the epoxy library.
#  LibEpoxy_ROOT_DIR, The base directory to search for libepoxy.
#                     This can also be an environment variable.
#  LibEpoxy_FOUND, If false, do not try to use libepoxy.

if(NOT EPOXY_ROOT_DIR AND NOT $ENV{EPOXY_ROOT_DIR} STREQUAL "")
  set(EPOXY_ROOT_DIR $ENV{EPOXY_ROOT_DIR})
endif()

find_path(LibEpoxy_INCLUDE_DIR
  NAMES
    epoxy/gl.h
  HINTS
    ${EPOXY_ROOT_DIR}
  PATH_SUFFIXES
    include
)

find_library(LibEpoxy_LIBRARY
  NAMES
    epoxy
  HINTS
    ${EPOXY_ROOT_DIR}
  PATH_SUFFIXES
    lib64 lib
)

# handle the QUIETLY and REQUIRED arguments and set LibEpoxy_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibEpoxy DEFAULT_MSG
    LibEpoxy_LIBRARY LibEpoxy_INCLUDE_DIR)

if(LibEpoxy_FOUND)
  set(LibEpoxy_INCLUDE_DIRS ${LibEpoxy_INCLUDE_DIR})
  set(LibEpoxy_LIBRARIES ${LibEpoxy_LIBRARY})
endif()

mark_as_advanced(
  LibEpoxy_INCLUDE_DIR
  LibEpoxy_LIBRARY
)
