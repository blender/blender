# SPDX-FileCopyrightText: 2015 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Eigen3 library
# Find the native Eigen3 includes and library
# This module defines
#  EIGEN3_INCLUDE_DIRS, where to find spnav.h, Set when
#                        EIGEN3_INCLUDE_DIR is found.
#  EIGEN3_ROOT_DIR, The base directory to search for Eigen3.
#                    This can also be an environment variable.
#  EIGEN3_FOUND, If false, do not try to use Eigen3.

# If `EIGEN3_ROOT_DIR` was defined in the environment, use it.
if(DEFINED EIGEN3_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{EIGEN3_ROOT_DIR})
  set(EIGEN3_ROOT_DIR $ENV{EIGEN3_ROOT_DIR})
else()
  set(EIGEN3_ROOT_DIR "")
endif()

set(_eigen3_SEARCH_DIRS
  ${EIGEN3_ROOT_DIR}
)

find_path(EIGEN3_INCLUDE_DIR
  NAMES
    # header has no '.h' suffix
    Eigen/Eigen
  HINTS
    ${_eigen3_SEARCH_DIRS}
  PATH_SUFFIXES
    include/eigen3
)

# handle the QUIETLY and REQUIRED arguments and set EIGEN3_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Eigen3 DEFAULT_MSG
    EIGEN3_INCLUDE_DIR)

if(EIGEN3_FOUND)
  set(EIGEN3_INCLUDE_DIRS ${EIGEN3_INCLUDE_DIR})
endif()

mark_as_advanced(
  EIGEN3_INCLUDE_DIR
)
