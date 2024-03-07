# SPDX-FileCopyrightText: 2022 Blender Authors
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
if(DEFINED BROTLI_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{BROTLI_ROOT_DIR})
  set(BROTLI_ROOT_DIR $ENV{BROTLI_ROOT_DIR})
else()
  set(BROTLI_ROOT_DIR "")
endif()

set(_brotli_SEARCH_DIRS
  ${BROTLI_ROOT_DIR}
)

find_path(BROTLI_INCLUDE_DIR
  NAMES
    brotli/decode.h
  HINTS
    ${_brotli_SEARCH_DIRS}
  PATH_SUFFIXES
    include
  DOC "Brotli header files"
)

find_library(BROTLI_LIBRARY_COMMON
  NAMES
    # Some builds use a special `-static` postfix in their static libraries names.
    brotlicommon-static
    brotlicommon
  HINTS
    ${_brotli_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib lib/static
  DOC "Brotli static common library"
)
find_library(BROTLI_LIBRARY_DEC
  NAMES
    # Some builds use a special `-static` postfix in their static libraries names.
    brotlidec-static
    brotlidec
  HINTS
    ${_brotli_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib lib/static
  DOC "Brotli static decode library"
)


if((NOT BROTLI_LIBRARY_COMMON) OR (NOT BROTLI_LIBRARY_DEC))
  set(BROTLI_FOUND FALSE)
else()
  # handle the QUIETLY and REQUIRED arguments and set BROTLI_FOUND to TRUE if
  # all listed variables are TRUE
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Brotli DEFAULT_MSG BROTLI_LIBRARY_COMMON BROTLI_LIBRARY_DEC BROTLI_INCLUDE_DIR)

  if(BROTLI_FOUND)
    get_filename_component(BROTLI_LIBRARY_DIR ${BROTLI_LIBRARY_COMMON} DIRECTORY)
    set(BROTLI_INCLUDE_DIRS ${BROTLI_INCLUDE_DIR})
    set(BROTLI_LIBRARIES ${BROTLI_LIBRARY_DEC} ${BROTLI_LIBRARY_COMMON})
  endif()
endif()

mark_as_advanced(
  BROTLI_INCLUDE_DIR
  BROTLI_LIBRARY_COMMON
  BROTLI_LIBRARY_DEC
  BROTLI_LIBRARY_DIR
)

unset(_brotli_SEARCH_DIRS)
