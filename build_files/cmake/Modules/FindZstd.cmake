# SPDX-FileCopyrightText: 2019 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Zstd library
# Find the native Zstd includes and library
# This module defines
#  ZSTD_INCLUDE_DIRS, where to find zstd.h, Set when
#                     ZSTD_INCLUDE_DIR is found.
#  ZSTD_LIBRARIES, libraries to link against to use Zstd.
#  ZSTD_ROOT_DIR, The base directory to search for Zstd.
#                 This can also be an environment variable.
#  ZSTD_FOUND, If false, do not try to use Zstd.
#
# also defined, but not for general use are
#  ZSTD_LIBRARY, where to find the Zstd library.

# If `ZSTD_ROOT_DIR` was defined in the environment, use it.
IF(DEFINED ZSTD_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{ZSTD_ROOT_DIR})
  SET(ZSTD_ROOT_DIR $ENV{ZSTD_ROOT_DIR})
ELSE()
  SET(ZSTD_ROOT_DIR "")
ENDIF()

SET(_zstd_SEARCH_DIRS
  ${ZSTD_ROOT_DIR}
)

FIND_PATH(ZSTD_INCLUDE_DIR
  NAMES
    zstd.h
  HINTS
    ${_zstd_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(ZSTD_LIBRARY
  NAMES
    zstd
  HINTS
    ${_zstd_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set ZSTD_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Zstd DEFAULT_MSG
    ZSTD_LIBRARY ZSTD_INCLUDE_DIR)

IF(ZSTD_FOUND)
  SET(ZSTD_LIBRARIES ${ZSTD_LIBRARY})
  SET(ZSTD_INCLUDE_DIRS ${ZSTD_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  ZSTD_INCLUDE_DIR
  ZSTD_LIBRARY
)
