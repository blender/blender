# SPDX-FileCopyrightText: 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Level Zero library
# Find Level Zero headers and libraries needed by oneAPI implementation
# This module defines
#  LEVEL_ZERO_LIBRARY, libraries to link against in order to use L0.
#  LEVEL_ZERO_INCLUDE_DIR, directories where L0 headers can be found.
#  LEVEL_ZERO_ROOT_DIR, The base directory to search for L0 files.
#                 This can also be an environment variable.
#  LEVEL_ZERO_FOUND, If false, then don't try to use L0.

IF(NOT LEVEL_ZERO_ROOT_DIR AND NOT $ENV{LEVEL_ZERO_ROOT_DIR} STREQUAL "")
  SET(LEVEL_ZERO_ROOT_DIR $ENV{LEVEL_ZERO_ROOT_DIR})
ENDIF()

SET(_level_zero_search_dirs
  ${LEVEL_ZERO_ROOT_DIR}
  /usr/lib
  /usr/local/lib
)

FIND_LIBRARY(_LEVEL_ZERO_LIBRARY
  NAMES
    ze_loader
  HINTS
    ${_level_zero_search_dirs}
  PATH_SUFFIXES
    lib64 lib
)

FIND_PATH(_LEVEL_ZERO_INCLUDE_DIR
  NAMES
    level_zero/ze_api.h
  HINTS
    ${_level_zero_search_dirs}
  PATH_SUFFIXES
    include
)

INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(LevelZero DEFAULT_MSG _LEVEL_ZERO_LIBRARY _LEVEL_ZERO_INCLUDE_DIR)

IF(LevelZero_FOUND)
  SET(LEVEL_ZERO_LIBRARY ${_LEVEL_ZERO_LIBRARY})
  SET(LEVEL_ZERO_INCLUDE_DIR ${_LEVEL_ZERO_INCLUDE_DIR} ${_LEVEL_ZERO_INCLUDE_PARENT_DIR})
  SET(LEVEL_ZERO_FOUND TRUE)
ELSE()
  SET(LEVEL_ZERO_FOUND FALSE)
ENDIF()

MARK_AS_ADVANCED(
  LEVEL_ZERO_LIBRARY
  LEVEL_ZERO_INCLUDE_DIR
)
