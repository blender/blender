# SPDX-FileCopyrightText: 2020 Blender Foundation
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find potrace library
# Find the potrace include and library
# This module defines
#  POTRACE_INCLUDE_DIRS, where to find potracelib.h, Set when
#                    POTRACE is found.
#  POTRACE_LIBRARIES, libraries to link against to use POTRACE.
#  POTRACE_ROOT_DIR, The base directory to search for POTRACE.
#                This can also be an environment variable.
#  POTRACE_FOUND, If false, do not try to use POTRACE.
#
# also defined, but not for general use are
#  POTRACE_LIBRARY, where to find the POTRACE library.

# If POTRACE_ROOT_DIR was defined in the environment, use it.
IF(NOT POTRACE_ROOT_DIR AND NOT $ENV{POTRACE_ROOT_DIR} STREQUAL "")
  SET(POTRACE_ROOT_DIR $ENV{POTRACE_ROOT_DIR})
ENDIF()

SET(_potrace_SEARCH_DIRS
  ${POTRACE_ROOT_DIR}
  /opt/lib/potrace
  /usr/include
  /usr/local/include
)

FIND_PATH(POTRACE_INCLUDE_DIR
  NAMES
    potracelib.h
  HINTS
    ${_potrace_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(POTRACE_LIBRARY
  NAMES
    potrace
  HINTS
    ${_potrace_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set POTRACE_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Potrace DEFAULT_MSG
    POTRACE_LIBRARY POTRACE_INCLUDE_DIR)

IF(POTRACE_FOUND)
  SET(POTRACE_LIBRARIES ${POTRACE_LIBRARY})
  SET(POTRACE_INCLUDE_DIRS ${POTRACE_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  POTRACE_INCLUDE_DIR
  POTRACE_LIBRARY
)
