# - Find GMP library
# Find the native GMP includes and library
# This module defines
#  GMP_INCLUDE_DIRS, where to find gmp.h, Set when
#                        GMP_INCLUDE_DIR is found.
#  GMP_LIBRARIES, libraries to link against to use GMP.
#  GMP_ROOT_DIR, The base directory to search for GMP.
#                    This can also be an environment variable.
#  GMP_FOUND, If false, do not try to use GMP.
#
# also defined, but not for general use are
#  GMP_LIBRARY, where to find the GMP library.

#=============================================================================
# Copyright 2011 Blender Foundation.
#
# Distributed under the OSI-approved BSD 3-Clause License,
# see accompanying file BSD-3-Clause-license.txt for details.
#=============================================================================

# If GMP_ROOT_DIR was defined in the environment, use it.
IF(NOT GMP_ROOT_DIR AND NOT $ENV{GMP_ROOT_DIR} STREQUAL "")
  SET(GMP_ROOT_DIR $ENV{GMP_ROOT_DIR})
ENDIF()

SET(_gmp_SEARCH_DIRS
  ${GMP_ROOT_DIR}
  /opt/lib/gmp
)

FIND_PATH(GMP_INCLUDE_DIR
  NAMES
    gmp.h
  HINTS
    ${_gmp_SEARCH_DIRS}
  PATH_SUFFIXES
    include/gmp
)

FIND_PATH(GMPXX_INCLUDE_DIR
  NAMES
    gmpxx.h
  HINTS
    ${_gmp_SEARCH_DIRS}
  PATH_SUFFIXES
    include/gmp
)

FIND_LIBRARY(GMP_LIBRARY
  NAMES
    gmp
  HINTS
    ${_gmp_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

FIND_LIBRARY(GMPXX_LIBRARY
  NAMES
    gmpxx
  HINTS
    ${_gmp_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

if(GMP_INCLUDE_DIR)
  SET(_version_regex "^#define[ \t]+__GNU_MP_VERSION[ \t]+\"([^\"]+)\".*")
  file(STRINGS "${GMP_INCLUDE_DIR}/gmp.h"
    GMP_VERSION REGEX "${_version_regex}")
  string(REGEX REPLACE "${_version_regex}" "\\1"
    GMP_VERSION "${GMP_VERSION}")
  unset(_version_regex)
endif()

# handle the QUIETLY and REQUIRED arguments and set GMP_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GMP DEFAULT_MSG
    GMP_LIBRARY GMPXX_LIBRARY GMP_INCLUDE_DIR GMPXX_INCLUDE_DIR)

IF(GMP_FOUND)
  SET(GMP_LIBRARIES ${GMP_LIBRARY} ${GMPXX_LIBRARY})
  SET(GMP_INCLUDE_DIRS ${GMP_INCLUDE_DIR} ${GMPXX_INCLUDE_DIR})
ENDIF(GMP_FOUND)

MARK_AS_ADVANCED(
  GMP_INCLUDE_DIR
  GMP_LIBRARY
  GMPXX_INCLUDE_DIR
  GMPXX_LIBRARY
)
