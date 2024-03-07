# SPDX-FileCopyrightText: 2021 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

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

# If `LIBPULSE_ROOT_DIR` was defined in the environment, use it.
if(DEFINED LIBPULSE_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{LIBPULSE_ROOT_DIR})
  set(LIBPULSE_ROOT_DIR $ENV{LIBPULSE_ROOT_DIR})
else()
  set(LIBPULSE_ROOT_DIR "")
endif()

set(_pulse_SEARCH_DIRS
  ${LIBPULSE_ROOT_DIR}
)

find_path(LIBPULSE_INCLUDE_DIR pulse/pulseaudio.h
  HINTS
    ${_pulse_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(LIBPULSE_LIBRARY
  NAMES
    pulse
  HINTS
    ${_pulse_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set PULSE_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Pulse DEFAULT_MSG
  LIBPULSE_LIBRARY LIBPULSE_INCLUDE_DIR)

if(PULSE_FOUND)
  set(LIBPULSE_LIBRARIES ${LIBPULSE_LIBRARY})
  set(LIBPULSE_INCLUDE_DIRS ${LIBPULSE_INCLUDE_DIR})
endif()

mark_as_advanced(
  LIBPULSE_INCLUDE_DIR
  LIBPULSE_LIBRARY
)

unset(_pulse_SEARCH_DIRS)
