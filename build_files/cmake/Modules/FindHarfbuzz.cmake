# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Harfbuzz library
# Find the native harfbuzz includes and library
# This module defines
#  LIBHARFBUZZ_INCLUDE_DIRS, where to find hb.h, Set when
#                         LIBHARFBUZZ_INCLUDE_DIR is found.
#  LIBHARFBUZZ_LIBRARIES, libraries to link against to use harfbuzz.
#  LIBHARFBUZZ_ROOT_DIR, The base directory to search for Harfbuzz.
#                     This can also be an environment variable.
#  HARFBUZZ_FOUND, If false, do not try to use Harfbuzz.
#
# also defined, but not for general use are
#  LIBHARFBUZZ_LIBRARY, where to find the Harfbuzz library.

# If LIBHARFBUZZ_ROOT_DIR was defined in the environment, use it.
IF(NOT LIBHARFBUZZ_ROOT_DIR AND NOT $ENV{LIBHARFBUZZ_ROOT_DIR} STREQUAL "")
  SET(LIBHARFBUZZ_ROOT_DIR $ENV{LIBHARFBUZZ_ROOT_DIR})
ENDIF()

SET(_harfbuzz_SEARCH_DIRS
  ${LIBHARFBUZZ_ROOT_DIR}
)

FIND_PATH(LIBHARFBUZZ_INCLUDE_DIR harfbuzz/hb.h
  HINTS
    ${_harfbuzz_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(LIBHARFBUZZ_LIBRARY
  NAMES
    harfbuzz
  HINTS
    ${_harfbuzz_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set HARFBUZZ_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Harfbuzz DEFAULT_MSG
  LIBHARFBUZZ_LIBRARY LIBHARFBUZZ_INCLUDE_DIR)

IF(HARFBUZZ_FOUND)
  SET(LIBHARFBUZZ_LIBRARIES ${LIBHARFBUZZ_LIBRARY})
  SET(LIBHARFBUZZ_INCLUDE_DIRS ${LIBHARFBUZZ_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  LIBHARFBUZZ_INCLUDE_DIR
  LIBHARFBUZZ_LIBRARY
)
