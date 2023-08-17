# SPDX-FileCopyrightText: 2015 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find OPENVDB library
# Find the native OPENVDB includes and library
# This module defines
#  OPENVDB_INCLUDE_DIRS, where to find openvdb.h, Set when
#                            OPENVDB_INCLUDE_DIR is found.
#  OPENVDB_LIBRARIES, libraries to link against to use OPENVDB.
#  OPENVDB_ROOT_DIR, The base directory to search for OPENVDB.
#                        This can also be an environment variable.
#  OPENVDB_FOUND, If false, do not try to use OPENVDB.
#
# also defined, but not for general use are
#  OPENVDB_LIBRARY, where to find the OPENVDB library.

# If `OPENVDB_ROOT_DIR` was defined in the environment, use it.
if(DEFINED OPENVDB_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{OPENVDB_ROOT_DIR})
  set(OPENVDB_ROOT_DIR $ENV{OPENVDB_ROOT_DIR})
else()
  set(OPENVDB_ROOT_DIR "")
endif()

set(_openvdb_SEARCH_DIRS
  ${OPENVDB_ROOT_DIR}
  /opt/lib/openvdb
)

find_path(OPENVDB_INCLUDE_DIR
  NAMES
    openvdb/openvdb.h
  HINTS
    ${_openvdb_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(OPENVDB_LIBRARY
  NAMES
    openvdb
  HINTS
    ${_openvdb_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
)

# handle the QUIETLY and REQUIRED arguments and set OPENVDB_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenVDB DEFAULT_MSG
    OPENVDB_LIBRARY OPENVDB_INCLUDE_DIR)

if(OPENVDB_FOUND)
  set(OPENVDB_LIBRARIES ${OPENVDB_LIBRARY})
  set(OPENVDB_INCLUDE_DIRS ${OPENVDB_INCLUDE_DIR})
endif()

mark_as_advanced(
  OPENVDB_INCLUDE_DIR
  OPENVDB_LIBRARY
)

unset(_openvdb_SEARCH_DIRS)
