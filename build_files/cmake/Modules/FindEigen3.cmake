# - Find Eigen3 library
# Find the native Eigen3 includes and library
# This module defines
#  EIGEN3_INCLUDE_DIRS, where to find spnav.h, Set when
#                        EIGEN3_INCLUDE_DIR is found.
#  EIGEN3_ROOT_DIR, The base directory to search for Eigen3.
#                    This can also be an environment variable.
#  EIGEN3_FOUND, If false, do not try to use Eigen3.
#
#=============================================================================
# Copyright 2015 Blender Foundation.
#
# Distributed under the OSI-approved BSD 3-Clause License,
# see accompanying file BSD-3-Clause-license.txt for details.
#=============================================================================

# If EIGEN3_ROOT_DIR was defined in the environment, use it.
IF(NOT EIGEN3_ROOT_DIR AND NOT $ENV{EIGEN3_ROOT_DIR} STREQUAL "")
  SET(EIGEN3_ROOT_DIR $ENV{EIGEN3_ROOT_DIR})
ENDIF()

SET(_eigen3_SEARCH_DIRS
  ${EIGEN3_ROOT_DIR}
)

FIND_PATH(EIGEN3_INCLUDE_DIR
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
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Eigen3 DEFAULT_MSG
    EIGEN3_INCLUDE_DIR)

IF(EIGEN3_FOUND)
  SET(EIGEN3_INCLUDE_DIRS ${EIGEN3_INCLUDE_DIR})
ENDIF(EIGEN3_FOUND)

MARK_AS_ADVANCED(
  EIGEN3_INCLUDE_DIR
)
