# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# This module defines
#  LibEpoxy_INCLUDE_DIRS, where to find epoxy/gl.h
#  LibEpoxy_LIBRARY, where to find the epoxy library.
#  LibEpoxy_ROOT_DIR, The base directory to search for libepoxy.
#                     This can also be an environment variable.
#  LibEpoxy_FOUND, If false, do not try to use libepoxy.

IF(NOT EPOXY_ROOT_DIR AND NOT $ENV{EPOXY_ROOT_DIR} STREQUAL "")
  SET(EPOXY_ROOT_DIR $ENV{EPOXY_ROOT_DIR})
ENDIF()

FIND_PATH(LibEpoxy_INCLUDE_DIR
  NAMES
    epoxy/gl.h
  HINTS
    ${EPOXY_ROOT_DIR}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(LibEpoxy_LIBRARY
  NAMES
    epoxy
  HINTS
    ${EPOXY_ROOT_DIR}
  PATH_SUFFIXES
    lib64 lib
)

# handle the QUIETLY and REQUIRED arguments and set LibEpoxy_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibEpoxy DEFAULT_MSG
    LibEpoxy_LIBRARY LibEpoxy_INCLUDE_DIR)

IF(LibEpoxy_FOUND)
  SET(LibEpoxy_INCLUDE_DIRS ${LibEpoxy_INCLUDE_DIR})
  SET(LibEpoxy_LIBRARIES ${LibEpoxy_LIBRARY})
ENDIF()

MARK_AS_ADVANCED(
  LibEpoxy_INCLUDE_DIR
  LibEpoxy_LIBRARY
)
