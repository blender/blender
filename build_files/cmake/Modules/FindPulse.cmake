# - Find PulseAudio library
# Find the native PulseAudio includes and library
# This module defines
#  LIBPULSE_INCLUDE_DIRS, where to find pulse/pulseaudio.h, Set when
#                         LIBPULSE_INCLUDE_DIR is found.
#  LIBPULSE_LIBRARIES, libraries to link against to use PulseAudio.
#  LIBPULSE_ROOT_DIR, The base directory to search for PulseAudio.
#                     This can also be an environment variable.
#  PULSE_FOUND, If false, do not try to use PulseAudio.
#
# also defined, but not for general use are
#  LIBPULSE_LIBRARY, where to find the PulseAudio library.

#=============================================================================
# Copyright 2021 Blender Foundation.
#
# Distributed under the OSI-approved BSD 3-Clause License,
# see accompanying file BSD-3-Clause-license.txt for details.
#=============================================================================

# If LIBPULSE_ROOT_DIR was defined in the environment, use it.
IF(NOT LIBPULSE_ROOT_DIR AND NOT $ENV{LIBPULSE_ROOT_DIR} STREQUAL "")
  SET(LIBPULSE_ROOT_DIR $ENV{LIBPULSE_ROOT_DIR})
ENDIF()

SET(_pulse_SEARCH_DIRS
  ${LIBPULSE_ROOT_DIR}
)

FIND_PATH(LIBPULSE_INCLUDE_DIR pulse/pulseaudio.h
  HINTS
    ${_pulse_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(LIBPULSE_LIBRARY
  NAMES
    pulse
  HINTS
    ${_pulse_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set PULSE_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Pulse DEFAULT_MSG
  LIBPULSE_LIBRARY LIBPULSE_INCLUDE_DIR)

IF(PULSE_FOUND)
  SET(LIBPULSE_LIBRARIES ${LIBPULSE_LIBRARY})
  SET(LIBPULSE_INCLUDE_DIRS ${LIBPULSE_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  LIBPULSE_INCLUDE_DIR
  LIBPULSE_LIBRARY
)
