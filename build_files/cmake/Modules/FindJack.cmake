# - Find JACK library
# Find the native JACK includes and library
# This module defines
#  JACK_INCLUDE_DIRS, where to find jack.h, Set when
#                        JACK_INCLUDE_DIR is found.
#  JACK_LIBRARIES, libraries to link against to use JACK.
#  JACK_ROOT_DIR, The base directory to search for JACK.
#                    This can also be an environment variable.
#  JACK_FOUND, If false, do not try to use JACK.
#
# also defined, but not for general use are
#  JACK_LIBRARY, where to find the JACK library.

#=============================================================================
# Copyright 2011 Blender Foundation.
#
# Distributed under the OSI-approved BSD 3-Clause License,
# see accompanying file BSD-3-Clause-license.txt for details.
#=============================================================================

# If JACK_ROOT_DIR was defined in the environment, use it.
IF(NOT JACK_ROOT_DIR AND NOT $ENV{JACK_ROOT_DIR} STREQUAL "")
  SET(JACK_ROOT_DIR $ENV{JACK_ROOT_DIR})
ENDIF()

SET(_jack_SEARCH_DIRS
  ${JACK_ROOT_DIR}
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
