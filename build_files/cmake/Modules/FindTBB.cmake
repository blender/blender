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

#=============================================================================
# Copyright 2016 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# If TBB_ROOT_DIR was defined in the environment, use it.
IF(NOT TBB_ROOT_DIR AND NOT $ENV{TBB_ROOT_DIR} STREQUAL "")
  SET(TBB_ROOT_DIR $ENV{TBB_ROOT_DIR})
ENDIF()

SET(_tbb_SEARCH_DIRS
  ${TBB_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt/lib/tbb
)

FIND_PATH(TBB_INCLUDE_DIR
  NAMES
    tbb/tbb.h
  HINTS
    ${_tbb_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(TBB_LIBRARY
  NAMES
    tbb
  HINTS
    ${_tbb_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set TBB_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TBB DEFAULT_MSG
    TBB_LIBRARY TBB_INCLUDE_DIR)

IF(TBB_FOUND)
  SET(TBB_LIBRARIES ${TBB_LIBRARY})
  SET(TBB_INCLUDE_DIRS ${TBB_INCLUDE_DIR})
ELSE()
  SET(TBB_TBB_FOUND FALSE)
ENDIF()

MARK_AS_ADVANCED(
  TBB_INCLUDE_DIR
  TBB_LIBRARY
)
