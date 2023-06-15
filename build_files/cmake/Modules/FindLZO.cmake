# SPDX-FileCopyrightText: 2015 Blender Foundation
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find LZO library
# Find the native LZO includes and library
# This module defines
#  LZO_INCLUDE_DIRS, where to find lzo1x.h, Set when
#                        LZO_INCLUDE_DIR is found.
#  LZO_LIBRARIES, libraries to link against to use LZO.
#  LZO_ROOT_DIR, The base directory to search for LZO.
#                    This can also be an environment variable.
#  LZO_FOUND, If false, do not try to use LZO.
#
# also defined, but not for general use are
#  LZO_LIBRARY, where to find the LZO library.

# If LZO_ROOT_DIR was defined in the environment, use it.
IF(NOT LZO_ROOT_DIR AND NOT $ENV{LZO_ROOT_DIR} STREQUAL "")
  SET(LZO_ROOT_DIR $ENV{LZO_ROOT_DIR})
ENDIF()

SET(_lzo_SEARCH_DIRS
  ${LZO_ROOT_DIR}
)

FIND_PATH(LZO_INCLUDE_DIR lzo/lzo1x.h
  HINTS
    ${_lzo_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(LZO_LIBRARY
  NAMES
    lzo2
  HINTS
    ${_lzo_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set LZO_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LZO DEFAULT_MSG
  LZO_LIBRARY LZO_INCLUDE_DIR)

IF(LZO_FOUND)
  SET(LZO_LIBRARIES ${LZO_LIBRARY})
  SET(LZO_INCLUDE_DIRS ${LZO_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  LZO_INCLUDE_DIR
  LZO_LIBRARY
)
