# SPDX-FileCopyrightText: 2021 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find HARU library
# Find the native Haru includes and library
# This module defines
#  HARU_INCLUDE_DIRS, where to find hpdf.h, set when
#                        HARU_INCLUDE_DIR is found.
#  HARU_LIBRARIES, libraries to link against to use Haru.
#  HARU_ROOT_DIR, The base directory to search for Haru.
#                    This can also be an environment variable.
#  HARU_FOUND, If false, do not try to use Haru.
#
# also defined, but not for general use are
#  HARU_LIBRARY, where to find the Haru library.

# If `HARU_ROOT_DIR` was defined in the environment, use it.
if(DEFINED HARU_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{HARU_ROOT_DIR})
  set(HARU_ROOT_DIR $ENV{HARU_ROOT_DIR})
else()
  set(HARU_ROOT_DIR "")
endif()

set(_haru_SEARCH_DIRS
  ${HARU_ROOT_DIR}
  /opt/lib/haru
)

find_path(HARU_INCLUDE_DIR
  NAMES
    hpdf.h
  HINTS
    ${_haru_SEARCH_DIRS}
  PATH_SUFFIXES
    include/haru
    include
)

find_library(HARU_LIBRARY
  NAMES
    hpdfs
    hpdf
  HINTS
    ${_haru_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
)

# Handle the QUIETLY and REQUIRED arguments and set HARU_FOUND to TRUE if
# all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Haru DEFAULT_MSG HARU_LIBRARY HARU_INCLUDE_DIR)

if(HARU_FOUND)
  set(HARU_LIBRARIES ${HARU_LIBRARY})
  set(HARU_INCLUDE_DIRS ${HARU_INCLUDE_DIR})
endif()

mark_as_advanced(
  HARU_INCLUDE_DIR
  HARU_LIBRARY
)

unset(_haru_SEARCH_DIRS)
