# SPDX-FileCopyrightText: 2011 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

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

# If `JACK_ROOT_DIR` was defined in the environment, use it.
if(DEFINED JACK_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{JACK_ROOT_DIR})
  set(JACK_ROOT_DIR $ENV{JACK_ROOT_DIR})
else()
  set(JACK_ROOT_DIR "")
endif()

set(_jack_SEARCH_DIRS
  ${JACK_ROOT_DIR}
)

find_path(JACK_INCLUDE_DIR
  NAMES
    jack.h
  HINTS
    ${_jack_SEARCH_DIRS}
  PATH_SUFFIXES
    include/jack
)

find_library(JACK_LIBRARY
  NAMES
    jack
  HINTS
    ${_jack_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set JACK_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jack DEFAULT_MSG
    JACK_LIBRARY JACK_INCLUDE_DIR)

if(JACK_FOUND)
  set(JACK_LIBRARIES ${JACK_LIBRARY})
  set(JACK_INCLUDE_DIRS ${JACK_INCLUDE_DIR})
endif()

mark_as_advanced(
  JACK_INCLUDE_DIR
  JACK_LIBRARY
)

unset(_jack_SEARCH_DIRS)
