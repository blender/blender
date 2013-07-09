# - Find Jack library
# Find the native Jack includes and library
# This module defines
#  JACK_INCLUDE_DIRS, where to find jack.h, Set when
#                        JACK_INCLUDE_DIR is found.
#  JACK_LIBRARIES, libraries to link against to use Jack.
#  JACK_ROOT_DIR, The base directory to search for Jack.
#                    This can also be an environment variable.
#  JACK_FOUND, If false, do not try to use Jack.
#
# also defined, but not for general use are
#  JACK_LIBRARY, where to find the Jack library.

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

# If JACK_ROOT_DIR was defined in the environment, use it.
IF(NOT JACK_ROOT_DIR AND NOT $ENV{JACK_ROOT_DIR} STREQUAL "")
  SET(JACK_ROOT_DIR $ENV{JACK_ROOT_DIR})
ENDIF()

SET(_jack_SEARCH_DIRS
  ${JACK_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
)

FIND_PATH(JACK_INCLUDE_DIR
  NAMES
    jack.h
  HINTS
    ${_jack_SEARCH_DIRS}
  PATH_SUFFIXES
    include/jack
)

FIND_LIBRARY(JACK_LIBRARY
  NAMES
    jack
  HINTS
    ${_jack_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set JACK_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Jack DEFAULT_MSG
    JACK_LIBRARY JACK_INCLUDE_DIR)

IF(JACK_FOUND)
  SET(JACK_LIBRARIES ${JACK_LIBRARY})
  SET(JACK_INCLUDE_DIRS ${JACK_INCLUDE_DIR})
ENDIF(JACK_FOUND)

MARK_AS_ADVANCED(
  JACK_INCLUDE_DIR
  JACK_LIBRARY
)
