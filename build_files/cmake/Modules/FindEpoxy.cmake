# SPDX-FileCopyrightText: 2022 Blender Foundation
#
# SPDX-License-Identifier: BSD-3-Clause

# This module defines
#  Epoxy_INCLUDE_DIRS, where to find epoxy/gl.h
#  Epoxy_LIBRARY, where to find the epoxy library.
#  Epoxy_ROOT_DIR, The base directory to search for epoxy.
#                     This can also be an environment variable.
#  Epoxy_FOUND, If false, do not try to use epoxy.

IF(NOT EPOXY_ROOT_DIR AND NOT $ENV{EPOXY_ROOT_DIR} STREQUAL "")
  SET(EPOXY_ROOT_DIR $ENV{EPOXY_ROOT_DIR})
ENDIF()

FIND_PATH(Epoxy_INCLUDE_DIR
  NAMES
    epoxy/gl.h
  HINTS
    ${EPOXY_ROOT_DIR}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(Epoxy_LIBRARY
  NAMES
    epoxy
  HINTS
    ${EPOXY_ROOT_DIR}
  PATH_SUFFIXES
    lib64 lib
)

# handle the QUIETLY and REQUIRED arguments and set Epoxy_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Epoxy DEFAULT_MSG
    Epoxy_LIBRARY Epoxy_INCLUDE_DIR)

IF(Epoxy_FOUND)
  SET(Epoxy_INCLUDE_DIRS ${Epoxy_INCLUDE_DIR})
  SET(Epoxy_LIBRARIES ${Epoxy_LIBRARY})
ENDIF()

MARK_AS_ADVANCED(
  Epoxy_INCLUDE_DIR
  Epoxy_LIBRARY
)
