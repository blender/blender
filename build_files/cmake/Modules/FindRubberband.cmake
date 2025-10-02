# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Rubberband libraries
# Find the Rubberband includes and libraries
# This module defines
#  RUBBERBAND_INCLUDE_DIRS, where to find Rubberband headers, Set when
#                       RUBBERBAND_INCLUDE_DIR is found.
#  RUBBERBAND_LIBRARIES, libraries to link against to use Rubberband.
#  RUBBERBAND_ROOT_DIR, The base directory to search for Rubberband.
#                    This can also be an environment variable.
#  RUBBERBAND_FOUND, If false, do not try to use the Rubberband library.
#
# also defined, but not for general use are
#  RUBBERBAND_LIBRARY, where to find the Rubberband library.

if(DEFINED RUBBERBAND_ROOT_DIR)
elseif(DEFINED ENV{RUBBERBAND_ROOT_DIR})
  set(RUBBERBAND_ROOT_DIR $ENV{RUBBERBAND_ROOT_DIR})
else()
  set(RUBBERBAND_ROOT_DIR "")
endif()

set(_rubberband_SEARCH_DIRS
  ${RUBBERBAND_ROOT_DIR}
)

find_path(RUBBERBAND_INCLUDE_DIR
  NAMES
    rubberband/RubberBandStretcher.h
  HINTS
    ${_rubberband_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(RUBBERBAND_LIBRARY
  NAMES
    rubberband
  HINTS
    ${_rubberband_SEARCH_DIRS}
  PATH_SUFFIXES
    lib lib64
)

# handle the QUIETLY and REQUIRED arguments and set RUBBERBAND_FOUND to TRUE if
# all listed variables are TRUE
find_package(PackageHandleStandardArgs)
find_package_handle_standard_args(Rubberband DEFAULT_MSG RUBBERBAND_LIBRARY RUBBERBAND_INCLUDE_DIR)

if(RUBBERBAND_FOUND)
  set(RUBBERBAND_LIBRARIES ${RUBBERBAND_LIBRARY})
  set(RUBBERBAND_INCLUDE_DIRS ${RUBBERBAND_INCLUDE_DIR})
endif()


mark_as_advanced(
  RUBBERBAND_LIBRARY
  RUBBERBAND_INCLUDE_DIR
)

unset(_rubberband_SEARCH_DIRS)
