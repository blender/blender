# SPDX-FileCopyrightText: 2011 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find PCRE library
# Find the native PCRE includes and library
# This module defines
#  PCRE_INCLUDE_DIRS, where to find pcre.h, Set when
#                     PCRE_INCLUDE_DIR is found.
#  PCRE_LIBRARIES, libraries to link against to use PCRE.
#  PCRE_ROOT_DIR, The base directory to search for PCRE.
#                 This can also be an environment variable.
#  PCRE_FOUND, If false, do not try to use PCRE.
#
# also defined, but not for general use are
#  PCRE_LIBRARY, where to find the PCRE library.

# If `PCRE_ROOT_DIR` was defined in the environment, use it.
if(DEFINED PCRE_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{PCRE_ROOT_DIR})
  set(PCRE_ROOT_DIR $ENV{PCRE_ROOT_DIR})
else()
  set(PCRE_ROOT_DIR "")
endif()

set(_pcre_SEARCH_DIRS
  ${PCRE_ROOT_DIR}
)

find_path(PCRE_INCLUDE_DIR pcre.h
  HINTS
    ${_pcre_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(PCRE_LIBRARY
  NAMES
    pcre
  HINTS
    ${_pcre_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set PCRE_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCRE DEFAULT_MSG
    PCRE_LIBRARY PCRE_INCLUDE_DIR)

# With 'make deps' precompiled libs, opencollada ships with a copy of libpcre
# but not the headers, ${PCRE_LIBRARY} will be valid in this case
# but PCRE_FOUND will be FALSE. So we set this variable outside of
# the `if(PCRE_FOUND)` below to allow blender to successfully link.
set(PCRE_LIBRARIES ${PCRE_LIBRARY})

if(PCRE_FOUND)
  set(PCRE_INCLUDE_DIRS ${PCRE_INCLUDE_DIR})
endif()

mark_as_advanced(
  PCRE_INCLUDE_DIR
  PCRE_LIBRARY
)
