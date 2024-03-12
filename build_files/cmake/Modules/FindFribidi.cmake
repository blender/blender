# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Fribidi library
# Find the native fribidi includes and library
# This module defines
#  LIBFRIBIDI_INCLUDE_DIRS, where to find hb.h, Set when
#                         LIBFRIBIDI_INCLUDE_DIR is found.
#  LIBFRIBIDI_LIBRARIES, libraries to link against to use fribidi.
#  LIBFRIBIDI_ROOT_DIR, The base directory to search for Fribidi.
#                     This can also be an environment variable.
#  FRIBIDI_FOUND, If false, do not try to use Fribidi.
#
# also defined, but not for general use are
#  LIBFRIBIDI_LIBRARY, where to find the Fribidi library.

# If LIBFRIBIDI_ROOT_DIR was defined in the environment, use it.
IF(NOT LIBFRIBIDI_ROOT_DIR AND NOT $ENV{LIBFRIBIDI_ROOT_DIR} STREQUAL "")
  SET(LIBFRIBIDI_ROOT_DIR $ENV{LIBFRIBIDI_ROOT_DIR})
ENDIF()

SET(_fribidi_SEARCH_DIRS
  ${LIBFRIBIDI_ROOT_DIR}
)

FIND_PATH(LIBFRIBIDI_INCLUDE_DIR fribidi/fribidi.h
  HINTS
    ${_fribidi_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(LIBFRIBIDI_LIBRARY
  NAMES
    fribidi
  HINTS
    ${_fribidi_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set FRIBIDI_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Fribidi DEFAULT_MSG
  LIBFRIBIDI_LIBRARY LIBFRIBIDI_INCLUDE_DIR)

IF(FRIBIDI_FOUND)
  SET(LIBFRIBIDI_LIBRARIES ${LIBFRIBIDI_LIBRARY})
  SET(LIBFRIBIDI_INCLUDE_DIRS ${LIBFRIBIDI_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  LIBFRIBIDI_INCLUDE_DIR
  LIBFRIBIDI_LIBRARY
)

unset(_fribidi_SEARCH_DIRS)
