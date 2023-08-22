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

# If `LEVEL_ZERO_ROOT_DIR` was defined in the environment, use it.
if(DEFINED LEVEL_ZERO_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{LEVEL_ZERO_ROOT_DIR})
  set(LEVEL_ZERO_ROOT_DIR $ENV{LEVEL_ZERO_ROOT_DIR})
else()
  set(LEVEL_ZERO_ROOT_DIR "")
endif()

set(_level_zero_search_dirs
  ${LEVEL_ZERO_ROOT_DIR}
  /usr/lib
  /usr/local/lib
)

find_library(_LEVEL_ZERO_LIBRARY
  NAMES
    ze_loader
  HINTS
    ${_level_zero_search_dirs}
  PATH_SUFFIXES
    lib64 lib
)

find_path(_LEVEL_ZERO_INCLUDE_DIR
  NAMES
    level_zero/ze_api.h
  HINTS
    ${_level_zero_search_dirs}
  PATH_SUFFIXES
    include
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LevelZero DEFAULT_MSG _LEVEL_ZERO_LIBRARY _LEVEL_ZERO_INCLUDE_DIR)

if(LevelZero_FOUND)
  set(LEVEL_ZERO_LIBRARY ${_LEVEL_ZERO_LIBRARY})
  set(LEVEL_ZERO_INCLUDE_DIR ${_LEVEL_ZERO_INCLUDE_DIR})
  set(LEVEL_ZERO_FOUND TRUE)
else()
  set(LEVEL_ZERO_FOUND FALSE)
endif()

mark_as_advanced(
  LEVEL_ZERO_LIBRARY
  LEVEL_ZERO_INCLUDE_DIR
)
