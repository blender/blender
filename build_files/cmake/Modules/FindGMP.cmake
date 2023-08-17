# SPDX-FileCopyrightText: 2011 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

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

# If `GMP_ROOT_DIR` was defined in the environment, use it.
if(DEFINED GMP_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{GMP_ROOT_DIR})
  set(GMP_ROOT_DIR $ENV{GMP_ROOT_DIR})
else()
  set(GMP_ROOT_DIR "")
endif()

set(_gmp_SEARCH_DIRS
  ${GMP_ROOT_DIR}
  /opt/lib/gmp
)

find_path(GMP_INCLUDE_DIR
  NAMES
    gmp.h
  HINTS
    ${_gmp_SEARCH_DIRS}
  PATH_SUFFIXES
    include/gmp
)

find_path(GMPXX_INCLUDE_DIR
  NAMES
    gmpxx.h
  HINTS
    ${_gmp_SEARCH_DIRS}
  PATH_SUFFIXES
    include/gmp
)

find_library(GMP_LIBRARY
  NAMES
    gmp
  HINTS
    ${_gmp_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

find_library(GMPXX_LIBRARY
  NAMES
    gmpxx
  HINTS
    ${_gmp_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

if(GMP_INCLUDE_DIR)
  set(_version_regex "^#define[ \t]+__GNU_MP_VERSION[ \t]+\"([^\"]+)\".*")
  file(STRINGS "${GMP_INCLUDE_DIR}/gmp.h"
    GMP_VERSION REGEX "${_version_regex}")
  string(REGEX REPLACE "${_version_regex}" "\\1"
    GMP_VERSION "${GMP_VERSION}")
  unset(_version_regex)
endif()

# handle the QUIETLY and REQUIRED arguments and set GMP_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP DEFAULT_MSG
    GMP_LIBRARY GMPXX_LIBRARY GMP_INCLUDE_DIR GMPXX_INCLUDE_DIR)

if(GMP_FOUND)
  set(GMP_LIBRARIES ${GMPXX_LIBRARY} ${GMP_LIBRARY})
  set(GMP_INCLUDE_DIRS ${GMP_INCLUDE_DIR} ${GMPXX_INCLUDE_DIR})
endif()

mark_as_advanced(
  GMP_INCLUDE_DIR
  GMP_LIBRARY
  GMPXX_INCLUDE_DIR
  GMPXX_LIBRARY
)
