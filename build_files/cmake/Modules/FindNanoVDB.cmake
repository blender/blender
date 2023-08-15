# SPDX-FileCopyrightText: 2020 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find NanoVDB library
# Find the native NanoVDB includes and library
# This module defines
#  NANOVDB_INCLUDE_DIRS, where to find nanovdb.h, Set when
#                         NANOVDB_INCLUDE_DIR is found.
#  NANOVDB_ROOT_DIR, The base directory to search for NanoVDB.
#                     This can also be an environment variable.
#  NANOVDB_FOUND, If false, do not try to use NanoVDB.

# If `NANOVDB_ROOT_DIR` was defined in the environment, use it.
IF(DEFINED NANOVDB_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{NANOVDB_ROOT_DIR})
  SET(NANOVDB_ROOT_DIR $ENV{NANOVDB_ROOT_DIR})
ELSE()
  SET(NANOVDB_ROOT_DIR "")
ENDIF()

SET(_nanovdb_SEARCH_DIRS
  ${NANOVDB_ROOT_DIR}
)

FIND_PATH(NANOVDB_INCLUDE_DIR
  NAMES
    nanovdb/NanoVDB.h
  HINTS
    ${_nanovdb_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

# handle the QUIETLY and REQUIRED arguments and set NANOVDB_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(NanoVDB DEFAULT_MSG
    NANOVDB_INCLUDE_DIR)

IF(NANOVDB_FOUND)
  SET(NANOVDB_INCLUDE_DIRS ${NANOVDB_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  NANOVDB_INCLUDE_DIR
)

UNSET(_nanovdb_SEARCH_DIRS)
