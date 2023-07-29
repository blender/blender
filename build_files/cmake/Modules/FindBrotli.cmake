# SPDX-FileCopyrightText: 2022 Blender Foundation
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Brotli library (compression for freetype/woff2).
# This module defines
#  BROTLI_INCLUDE_DIRS, where to find Brotli headers, Set when
#                       BROTLI_INCLUDE_DIR is found.
#  BROTLI_LIBRARIES, libraries to link against to use Brotli.
#  BROTLI_ROOT_DIR, The base directory to search for Brotli.
#                   This can also be an environment variable.
#  BROTLI_FOUND, If false, do not try to use Brotli.
#

# If `BROTLI_ROOT_DIR` was defined in the environment, use it.
IF(DEFINED BROTLI_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{BROTLI_ROOT_DIR})
  SET(BROTLI_ROOT_DIR $ENV{BROTLI_ROOT_DIR})
ELSE()
  SET(BROTLI_ROOT_DIR "")
ENDIF()

SET(_BROTLI_SEARCH_DIRS
  ${BROTLI_ROOT_DIR}
)

FIND_PATH(BROTLI_INCLUDE_DIR
  NAMES
    brotli/decode.h
  HINTS
    ${_BROTLI_SEARCH_DIRS}
  PATH_SUFFIXES
    include
  DOC "Brotli header files"
)

FIND_LIBRARY(BROTLI_LIBRARY_COMMON
  NAMES
    # Some builds use a special `-static` postfix in their static libraries names.
    brotlicommon-static
    brotlicommon
  HINTS
    ${_BROTLI_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib lib/static
  DOC "Brotli static common library"
)
FIND_LIBRARY(BROTLI_LIBRARY_DEC
  NAMES
    # Some builds use a special `-static` postfix in their static libraries names.
    brotlidec-static
    brotlidec
  HINTS
    ${_BROTLI_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib lib/static
  DOC "Brotli static decode library"
)


IF(${BROTLI_LIBRARY_COMMON_NOTFOUND} or ${BROTLI_LIBRARY_DEC_NOTFOUND})
  set(BROTLI_FOUND FALSE)
ELSE()
  # handle the QUIETLY and REQUIRED arguments and set BROTLI_FOUND to TRUE if
  # all listed variables are TRUE
  INCLUDE(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(Brotli DEFAULT_MSG BROTLI_LIBRARY_COMMON BROTLI_LIBRARY_DEC BROTLI_INCLUDE_DIR)

  IF(BROTLI_FOUND)
    get_filename_component(BROTLI_LIBRARY_DIR ${BROTLI_LIBRARY_COMMON} DIRECTORY)
    SET(BROTLI_INCLUDE_DIRS ${BROTLI_INCLUDE_DIR})
    SET(BROTLI_LIBRARIES ${BROTLI_LIBRARY_DEC} ${BROTLI_LIBRARY_COMMON})
  ENDIF()
ENDIF()

MARK_AS_ADVANCED(
  BROTLI_INCLUDE_DIR
  BROTLI_LIBRARY_COMMON
  BROTLI_LIBRARY_DEC
  BROTLI_LIBRARY_DIR
)

UNSET(_BROTLI_SEARCH_DIRS)
