# SPDX-FileCopyrightText: 2016 Blender Foundation
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find LOGC4PLUS library
# Find the native LOGC4PLUS includes and library
# This module defines
#  LOGC4PLUS_INCLUDE_DIRS, where to find logc4plus.h, Set when
#                    LOGC4PLUS is found.
#  LOGC4PLUS_LIBRARIES, libraries to link against to use LOGC4PLUS.
#  LOGC4PLUS_ROOT_DIR, The base directory to search for LOGC4PLUS.
#                This can also be an environment variable.
#  LOGC4PLUS_FOUND, If false, do not try to use LOGC4PLUS.
#
# also defined, but not for general use are
#  LOGC4PLUS_LIBRARY, where to find the LOGC4PLUS library.

# If LOGC4PLUS_ROOT_DIR was defined in the environment, use it.
IF(NOT LOGC4PLUS_ROOT_DIR AND NOT $ENV{LOGC4PLUS_ROOT_DIR} STREQUAL "")
  SET(LOGC4PLUS_ROOT_DIR $ENV{LOGC4PLUS_ROOT_DIR})
ENDIF()

SET(_logc4plus_SEARCH_DIRS
  ${LOGC4PLUS_ROOT_DIR}
  /opt/lib/logc4plus
)

FIND_PATH(LOGC4PLUS_INCLUDE_DIR
  NAMES
    logc4plus.h
  HINTS
    ${_logc4plus_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(LOGC4PLUS_LIBRARY
  NAMES
    logc4plus
  HINTS
    ${_logc4plus_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set LOGC4PLUS_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LOGC4PLUS DEFAULT_MSG
    LOGC4PLUS_LIBRARY LOGC4PLUS_INCLUDE_DIR)

IF(LOGC4PLUS_FOUND)
  SET(LOGC4PLUS_LIBRARIES ${LOGC4PLUS_LIBRARY})
  SET(LOGC4PLUS_INCLUDE_DIRS ${LOGC4PLUS_INCLUDE_DIR})
ELSE()
  SET(LOGC4PLUS_LOGC4PLUS_FOUND FALSE)
ENDIF()

MARK_AS_ADVANCED(
  LOGC4PLUS_INCLUDE_DIR
  LOGC4PLUS_LIBRARY
)
