# SPDX-FileCopyrightText: 2016 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find TBB library
# Find the native TBB includes and library
# This module defines
#  TBB_INCLUDE_DIRS, where to find tbb.h, Set when
#                    TBB is found.
#  TBB_LIBRARIES, libraries to link against to use TBB.
#  TBB_ROOT_DIR, The base directory to search for TBB.
#                This can also be an environment variable.
#  TBB_FOUND, If false, do not try to use TBB.
#
# also defined, but not for general use are
#  TBB_LIBRARY, where to find the TBB library.

# If `TBB_ROOT_DIR` was defined in the environment, use it.
if(DEFINED TBB_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{TBB_ROOT_DIR})
  set(TBB_ROOT_DIR $ENV{TBB_ROOT_DIR})
else()
  set(TBB_ROOT_DIR "")
endif()

set(_tbb_SEARCH_DIRS
  ${TBB_ROOT_DIR}
  /opt/lib/tbb
)

find_path(TBB_INCLUDE_DIR
  NAMES
    tbb/tbb.h
  HINTS
    ${_tbb_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(TBB_LIBRARY
  NAMES
    tbb
  HINTS
    ${_tbb_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set TBB_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TBB DEFAULT_MSG
    TBB_LIBRARY TBB_INCLUDE_DIR)

if(TBB_FOUND)
  set(TBB_LIBRARIES ${TBB_LIBRARY})
  set(TBB_INCLUDE_DIRS ${TBB_INCLUDE_DIR})
else()
  set(TBB_TBB_FOUND FALSE)
endif()

mark_as_advanced(
  TBB_INCLUDE_DIR
  TBB_LIBRARY
)

unset(_tbb_SEARCH_DIRS)
