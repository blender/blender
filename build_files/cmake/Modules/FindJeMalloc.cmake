# SPDX-FileCopyrightText: 2011 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

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

# If `JEMALLOC_ROOT_DIR` was defined in the environment, use it.
if(DEFINED JEMALLOC_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{JEMALLOC_ROOT_DIR})
  set(JEMALLOC_ROOT_DIR $ENV{JEMALLOC_ROOT_DIR})
else()
  set(JEMALLOC_ROOT_DIR "")
endif()

set(_jemalloc_SEARCH_DIRS
  ${JEMALLOC_ROOT_DIR}
  /opt/lib/jemalloc
)

find_path(JEMALLOC_INCLUDE_DIR
  NAMES
    jemalloc.h
  HINTS
    ${_jemalloc_SEARCH_DIRS}
  PATH_SUFFIXES
    include/jemalloc
)

find_library(JEMALLOC_LIBRARY
  NAMES
    jemalloc
  HINTS
    ${_jemalloc_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

if(JEMALLOC_INCLUDE_DIR)
  set(_version_regex "^#define[ \t]+JEMALLOC_VERSION[ \t]+\"([^\"]+)\".*")
  file(STRINGS "${JEMALLOC_INCLUDE_DIR}/jemalloc.h"
    JEMALLOC_VERSION REGEX "${_version_regex}")
  string(REGEX REPLACE "${_version_regex}" "\\1"
    JEMALLOC_VERSION "${JEMALLOC_VERSION}")
  unset(_version_regex)
endif()

# handle the QUIETLY and REQUIRED arguments and set JEMALLOC_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JeMalloc DEFAULT_MSG
    JEMALLOC_LIBRARY JEMALLOC_INCLUDE_DIR)

if(JEMALLOC_FOUND)
  set(JEMALLOC_LIBRARIES ${JEMALLOC_LIBRARY})
  set(JEMALLOC_INCLUDE_DIRS ${JEMALLOC_INCLUDE_DIR})
endif()

mark_as_advanced(
  JEMALLOC_INCLUDE_DIR
  JEMALLOC_LIBRARY
)

unset(_jemalloc_SEARCH_DIRS)
