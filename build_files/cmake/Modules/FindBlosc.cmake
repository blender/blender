# SPDX-FileCopyrightText: 2018 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Blosc library
# Find the native Blosc includes and library
# This module defines
#  BLOSC_INCLUDE_DIRS, where to find blosc.h, Set when
#                    Blosc is found.
#  BLOSC_LIBRARIES, libraries to link against to use Blosc.
#  BLOSC_ROOT_DIR, The base directory to search for Blosc.
#                This can also be an environment variable.
#  BLOSC_FOUND, If false, do not try to use Blosc.
#
# also defined, but not for general use are
#  BLOSC_LIBRARY, where to find the Blosc library.

# If `BLOSC_ROOT_DIR` was defined in the environment, use it.
if(DEFINED BLOSC_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{BLOSC_ROOT_DIR})
  set(BLOSC_ROOT_DIR $ENV{BLOSC_ROOT_DIR})
else()
  set(BLOSC_ROOT_DIR "")
endif()

set(_blosc_SEARCH_DIRS
  ${BLOSC_ROOT_DIR}
  /opt/lib/blosc
)

find_path(BLOSC_INCLUDE_DIR
  NAMES
    blosc.h
  HINTS
    ${_blosc_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(BLOSC_LIBRARY
  NAMES
    blosc
  HINTS
    ${_blosc_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set BLOSC_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Blosc DEFAULT_MSG
    BLOSC_LIBRARY BLOSC_INCLUDE_DIR)

if(BLOSC_FOUND)
  set(BLOSC_LIBRARIES ${BLOSC_LIBRARY})
  set(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIR})
else()
  set(BLOSC_BLOSC_FOUND FALSE)
endif()

mark_as_advanced(
  BLOSC_INCLUDE_DIR
  BLOSC_LIBRARY
)

unset(_blosc_SEARCH_DIRS)
