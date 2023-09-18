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
if(DEFINED ZSTD_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{ZSTD_ROOT_DIR})
  set(ZSTD_ROOT_DIR $ENV{ZSTD_ROOT_DIR})
else()
  set(ZSTD_ROOT_DIR "")
endif()

set(_zstd_SEARCH_DIRS
  ${ZSTD_ROOT_DIR}
)

find_path(ZSTD_INCLUDE_DIR
  NAMES
    zstd.h
  HINTS
    ${_zstd_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(ZSTD_LIBRARY
  NAMES
    zstd
  HINTS
    ${_zstd_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set ZSTD_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Zstd DEFAULT_MSG
    ZSTD_LIBRARY ZSTD_INCLUDE_DIR)

if(ZSTD_FOUND)
  set(ZSTD_LIBRARIES ${ZSTD_LIBRARY})
  set(ZSTD_INCLUDE_DIRS ${ZSTD_INCLUDE_DIR})
endif()

mark_as_advanced(
  ZSTD_INCLUDE_DIR
  ZSTD_LIBRARY
)
