# SPDX-FileCopyrightText: 2018 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Blosc library
# Find the native Blosc includes and library
# This module defines
#  BLOSC_INCLUDE_DIRS, where to find blosc.h, Set when
#                    Blosc is found.
#  BLOSC_LIBRARIES, libraries to link against to use Blosc.
#  BLOSC_ROOT_DIR, The base directory to search for Blosc.
#                This can also be an environment variable.
#  BLOSC_FOUND, If false, do not try to use Blosc.
#
# also defined, but not for general use are
#  BLOSC_LIBRARY, where to find the Blosc library.

# If `BLOSC_ROOT_DIR` was defined in the environment, use it.
IF(DEFINED BLOSC_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{BLOSC_ROOT_DIR})
  SET(BLOSC_ROOT_DIR $ENV{BLOSC_ROOT_DIR})
ELSE()
  SET(BLOSC_ROOT_DIR "")
ENDIF()

SET(_blosc_SEARCH_DIRS
  ${BLOSC_ROOT_DIR}
  /opt/lib/blosc
)

FIND_PATH(BLOSC_INCLUDE_DIR
  NAMES
    blosc.h
  HINTS
    ${_blosc_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(BLOSC_LIBRARY
  NAMES
    blosc
  HINTS
    ${_blosc_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set BLOSC_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Blosc DEFAULT_MSG
    BLOSC_LIBRARY BLOSC_INCLUDE_DIR)

IF(BLOSC_FOUND)
  SET(BLOSC_LIBRARIES ${BLOSC_LIBRARY})
  SET(BLOSC_INCLUDE_DIRS ${BLOSC_INCLUDE_DIR})
ELSE()
  SET(BLOSC_BLOSC_FOUND FALSE)
ENDIF()

MARK_AS_ADVANCED(
  BLOSC_INCLUDE_DIR
  BLOSC_LIBRARY
)
