# - Find Samplerate library
# Find the native Samplerate includes and library
# This module defines
#  SAMPLERATE_INCLUDE_DIRS, where to find samplerate.h, Set when
#                        SAMPLERATE_INCLUDE_DIR is found.
#  SAMPLERATE_LIBRARIES, libraries to link against to use Samplerate.
#  SAMPLERATE_ROOT_DIR, The base directory to search for Samplerate.
#                    This can also be an environment variable.
#  SAMPLERATE_FOUND, If false, do not try to use Samplerate.
#
# also defined, but not for general use are
#  SAMPLERATE_LIBRARY, where to find the Samplerate library.

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

# If SAMPLERATE_ROOT_DIR was defined in the environment, use it.
IF(NOT SAMPLERATE_ROOT_DIR AND NOT $ENV{SAMPLERATE_ROOT_DIR} STREQUAL "")
  SET(SAMPLERATE_ROOT_DIR $ENV{SAMPLERATE_ROOT_DIR})
ENDIF()

SET(_samplerate_SEARCH_DIRS
  ${SAMPLERATE_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
)

FIND_PATH(SAMPLERATE_INCLUDE_DIR
  NAMES
    samplerate.h
  HINTS
    ${_samplerate_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(SAMPLERATE_LIBRARY
  NAMES
    samplerate
  HINTS
    ${_samplerate_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set SAMPLERATE_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Samplerate DEFAULT_MSG
    SAMPLERATE_LIBRARY SAMPLERATE_INCLUDE_DIR)

IF(SAMPLERATE_FOUND)
  SET(SAMPLERATE_LIBRARIES ${SAMPLERATE_LIBRARY})
  SET(SAMPLERATE_INCLUDE_DIRS ${SAMPLERATE_INCLUDE_DIR})
ENDIF(SAMPLERATE_FOUND)

MARK_AS_ADVANCED(
  SAMPLERATE_INCLUDE_DIR
  SAMPLERATE_LIBRARY
)
