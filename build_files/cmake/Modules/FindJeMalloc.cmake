# - Find JeMalloc library
# Find the native JeMalloc includes and library
# This module defines
#  JEMALLOC_INCLUDE_DIRS, where to find jemalloc.h, Set when
#                        JEMALLOC_INCLUDE_DIR is found.
#  JEMALLOC_LIBRARIES, libraries to link against to use JeMalloc.
#  JEMALLOC_ROOT_DIR, The base directory to search for JeMalloc.
#                    This can also be an environment variable.
#  JEMALLOC_FOUND, If false, do not try to use JeMalloc.
#
# also defined, but not for general use are
#  JEMALLOC_LIBRARY, where to find the JeMalloc library.

#=============================================================================
# Copyright 2011 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# If JEMALLOC_ROOT_DIR was defined in the environment, use it.
IF(NOT JEMALLOC_ROOT_DIR AND NOT $ENV{JEMALLOC_ROOT_DIR} STREQUAL "")
  SET(JEMALLOC_ROOT_DIR $ENV{JEMALLOC_ROOT_DIR})
ENDIF()

SET(_jemalloc_SEARCH_DIRS
  ${JEMALLOC_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
)

FIND_PATH(JEMALLOC_INCLUDE_DIR
  NAMES
    jemalloc.h
  HINTS
    ${_jemalloc_SEARCH_DIRS}
  PATH_SUFFIXES
    include/jemalloc
)

FIND_LIBRARY(JEMALLOC_LIBRARY
  NAMES
    jemalloc
  HINTS
    ${_jemalloc_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set JEMALLOC_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(JeMalloc DEFAULT_MSG
    JEMALLOC_LIBRARY JEMALLOC_INCLUDE_DIR)

IF(JEMALLOC_FOUND)
  SET(JEMALLOC_LIBRARIES ${JEMALLOC_LIBRARY})
  SET(JEMALLOC_INCLUDE_DIRS ${JEMALLOC_INCLUDE_DIR})
ENDIF(JEMALLOC_FOUND)

MARK_AS_ADVANCED(
  JEMALLOC_INCLUDE_DIR
  JEMALLOC_LIBRARY
)
