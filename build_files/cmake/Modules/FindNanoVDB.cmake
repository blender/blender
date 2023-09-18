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
if(DEFINED NANOVDB_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{NANOVDB_ROOT_DIR})
  set(NANOVDB_ROOT_DIR $ENV{NANOVDB_ROOT_DIR})
else()
  set(NANOVDB_ROOT_DIR "")
endif()

set(_nanovdb_SEARCH_DIRS
  ${NANOVDB_ROOT_DIR}
)

find_path(NANOVDB_INCLUDE_DIR
  NAMES
    nanovdb/NanoVDB.h
  HINTS
    ${_nanovdb_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

# handle the QUIETLY and REQUIRED arguments and set NANOVDB_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NanoVDB DEFAULT_MSG
    NANOVDB_INCLUDE_DIR)

if(NANOVDB_FOUND)
  set(NANOVDB_INCLUDE_DIRS ${NANOVDB_INCLUDE_DIR})
endif()

mark_as_advanced(
  NANOVDB_INCLUDE_DIR
)

unset(_nanovdb_SEARCH_DIRS)
