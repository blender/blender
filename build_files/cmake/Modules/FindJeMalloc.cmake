# SPDX-FileCopyrightText: 2011 Blender Foundation
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
IF(DEFINED JEMALLOC_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{JEMALLOC_ROOT_DIR})
  SET(JEMALLOC_ROOT_DIR $ENV{JEMALLOC_ROOT_DIR})
ELSE()
  SET(JEMALLOC_ROOT_DIR "")
ENDIF()

SET(_jemalloc_SEARCH_DIRS
  ${JEMALLOC_ROOT_DIR}
  /opt/lib/jemalloc
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

if(JEMALLOC_INCLUDE_DIR)
  SET(_version_regex "^#define[ \t]+JEMALLOC_VERSION[ \t]+\"([^\"]+)\".*")
  file(STRINGS "${JEMALLOC_INCLUDE_DIR}/jemalloc.h"
    JEMALLOC_VERSION REGEX "${_version_regex}")
  string(REGEX REPLACE "${_version_regex}" "\\1"
    JEMALLOC_VERSION "${JEMALLOC_VERSION}")
  unset(_version_regex)
endif()

# handle the QUIETLY and REQUIRED arguments and set JEMALLOC_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(JeMalloc DEFAULT_MSG
    JEMALLOC_LIBRARY JEMALLOC_INCLUDE_DIR)

IF(JEMALLOC_FOUND)
  SET(JEMALLOC_LIBRARIES ${JEMALLOC_LIBRARY})
  SET(JEMALLOC_INCLUDE_DIRS ${JEMALLOC_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  JEMALLOC_INCLUDE_DIR
  JEMALLOC_LIBRARY
)
