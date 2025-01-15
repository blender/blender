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
if(NOT LIBHARFBUZZ_ROOT_DIR AND NOT $ENV{LIBHARFBUZZ_ROOT_DIR} STREQUAL "")
  set(LIBHARFBUZZ_ROOT_DIR $ENV{LIBHARFBUZZ_ROOT_DIR})
endif()

set(_harfbuzz_SEARCH_DIRS
  ${LIBHARFBUZZ_ROOT_DIR}
)

find_path(LIBHARFBUZZ_INCLUDE_DIR harfbuzz/hb.h
  HINTS
    ${_harfbuzz_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(LIBHARFBUZZ_LIBRARY
  NAMES
    harfbuzz
  HINTS
    ${_harfbuzz_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
)

# handle the QUIETLY and REQUIRED arguments and set HARFBUZZ_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Harfbuzz DEFAULT_MSG
  LIBHARFBUZZ_LIBRARY LIBHARFBUZZ_INCLUDE_DIR)

if(HARFBUZZ_FOUND)
  set(LIBHARFBUZZ_LIBRARIES ${LIBHARFBUZZ_LIBRARY})
  set(LIBHARFBUZZ_INCLUDE_DIRS ${LIBHARFBUZZ_INCLUDE_DIR})
endif()

mark_as_advanced(
  LIBHARFBUZZ_INCLUDE_DIR
  LIBHARFBUZZ_LIBRARY
)

unset(_harfbuzz_SEARCH_DIRS)
