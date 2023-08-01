# SPDX-FileCopyrightText: 2020 Blender Foundation
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find sse2neon library
# Find the native sse2neon includes and library
# This module defines
#  SSE2NEON_INCLUDE_DIRS, where to find sse2neon.h, Set when
#                         SSE2NEON_INCLUDE_DIR is found.
#  SSE2NEON_ROOT_DIR, The base directory to search for sse2neon.
#                     This can also be an environment variable.
#  SSE2NEON_FOUND, If false, do not try to use sse2neon.

# If `SSE2NEON_ROOT_DIR` was defined in the environment, use it.
IF(DEFINED SSE2NEON_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{SSE2NEON_ROOT_DIR})
  SET(SSE2NEON_ROOT_DIR $ENV{SSE2NEON_ROOT_DIR})
ELSE()
  SET(SSE2NEON_ROOT_DIR "")
ENDIF()

SET(_sse2neon_SEARCH_DIRS
  ${SSE2NEON_ROOT_DIR}
)

FIND_PATH(SSE2NEON_INCLUDE_DIR
  NAMES
    sse2neon.h
  HINTS
    ${_sse2neon_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

# handle the QUIETLY and REQUIRED arguments and set SSE2NEON_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(sse2neon DEFAULT_MSG
    SSE2NEON_INCLUDE_DIR)

IF(SSE2NEON_FOUND)
  SET(SSE2NEON_INCLUDE_DIRS ${SSE2NEON_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  SSE2NEON_INCLUDE_DIR
)

UNSET(_sse2neon_SEARCH_DIRS)
