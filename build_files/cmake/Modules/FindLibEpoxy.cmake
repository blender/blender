# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# This module defines
#  LIBEPOXY_INCLUDE_DIRS, where to find epoxy/gl.h
#  LIBEPOXY_LIBRARIES, where to find the epoxy library.
#  LIBEPOXY_ROOT_DIR, The base directory to search for libepoxy.
#                     This can also be an environment variable.
#  LIBEPOXY_FOUND, If false, do not try to use libepoxy.

if(NOT EPOXY_ROOT_DIR AND NOT $ENV{EPOXY_ROOT_DIR} STREQUAL "")
  set(EPOXY_ROOT_DIR $ENV{EPOXY_ROOT_DIR})
endif()

find_path(LIBEPOXY_INCLUDE_DIR
  NAMES
    epoxy/gl.h
  HINTS
    ${EPOXY_ROOT_DIR}
  PATH_SUFFIXES
    include
)

find_library(LIBEPOXY_LIBRARY
  NAMES
    epoxy
  HINTS
    ${EPOXY_ROOT_DIR}
  PATH_SUFFIXES
    lib64 lib
)

# handle the QUIETLY and REQUIRED arguments and set LIBEPOXY_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibEpoxy DEFAULT_MSG
    LIBEPOXY_LIBRARY LIBEPOXY_INCLUDE_DIR)

if(LIBEPOXY_FOUND)
  set(LIBEPOXY_INCLUDE_DIRS ${LIBEPOXY_INCLUDE_DIR})
  set(LIBEPOXY_LIBRARIES ${LIBEPOXY_LIBRARY})
endif()

mark_as_advanced(
  LIBEPOXY_INCLUDE_DIR
  LIBEPOXY_LIBRARY
)
