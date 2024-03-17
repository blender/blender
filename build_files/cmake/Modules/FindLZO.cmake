# SPDX-FileCopyrightText: 2015 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find LZO library
# Find the native LZO includes and library
# This module defines
#  LZO_INCLUDE_DIRS, where to find lzo1x.h, Set when
#                        LZO_INCLUDE_DIR is found.
#  LZO_LIBRARIES, libraries to link against to use LZO.
#  LZO_ROOT_DIR, The base directory to search for LZO.
#                    This can also be an environment variable.
#  LZO_FOUND, If false, do not try to use LZO.
#
# also defined, but not for general use are
#  LZO_LIBRARY, where to find the LZO library.

# If `LZO_ROOT_DIR` was defined in the environment, use it.
if(DEFINED LZO_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{LZO_ROOT_DIR})
  set(LZO_ROOT_DIR $ENV{LZO_ROOT_DIR})
else()
  set(LZO_ROOT_DIR "")
endif()

set(_lzo_SEARCH_DIRS
  ${LZO_ROOT_DIR}
)

find_path(LZO_INCLUDE_DIR lzo/lzo1x.h
  HINTS
    ${_lzo_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(LZO_LIBRARY
  NAMES
    lzo2
  HINTS
    ${_lzo_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set LZO_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LZO DEFAULT_MSG
  LZO_LIBRARY LZO_INCLUDE_DIR)

if(LZO_FOUND)
  set(LZO_LIBRARIES ${LZO_LIBRARY})
  set(LZO_INCLUDE_DIRS ${LZO_INCLUDE_DIR})
endif()

mark_as_advanced(
  LZO_INCLUDE_DIR
  LZO_LIBRARY
)

unset(_lzo_SEARCH_DIRS)
