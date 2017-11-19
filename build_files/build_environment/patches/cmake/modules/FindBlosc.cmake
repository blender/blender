# - Find BLOSC library
# Find the native BLOSC includes and library
# This module defines
#  BLOSC_INCLUDE_DIRS, where to find blosc.h, Set when
#                    BLOSC is found.
#  BLOSC_LIBRARIES, libraries to link against to use BLOSC.
#  BLOSC_ROOT_DIR, The base directory to search for BLOSC.
#                This can also be an environment variable.
#  BLOSC_FOUND, If false, do not try to use BLOSC.
#
# also defined, but not for general use are
#  BLOSC_LIBRARY, where to find the BLOSC library.

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

# If BLOSC_ROOT_DIR was defined in the environment, use it.
IF(NOT BLOSC_ROOT_DIR AND NOT $ENV{BLOSC_ROOT_DIR} STREQUAL "")
  SET(BLOSC_ROOT_DIR $ENV{BLOSC_ROOT_DIR})
ENDIF()

SET(_blosc_SEARCH_DIRS
  ${BLOSC_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/lib/blosc
)

FIND_PATH(BLOSC_INCLUDE_DIR
  NAMES
    blosc.h
  HINTS
    ${_blosc_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(BLOSC_LIBRARY
  NAMES
    blosc
  HINTS
    ${_blosc_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
)

# handle the QUIETLY and REQUIRED arguments and set BLOSC_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(BLOSC DEFAULT_MSG
    BLOSC_LIBRARY BLOSC_INCLUDE_DIR)

IF(BLOSC_FOUND)
  SET(BLOSC_LIBRARIES ${BLOSC_LIBRARY})
  SET(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIR})
ELSE()
  SET(BLOSC_FOUND FALSE)
ENDIF()

MARK_AS_ADVANCED(
  BLOSC_INCLUDE_DIR
  BLOSC_LIBRARY
)
