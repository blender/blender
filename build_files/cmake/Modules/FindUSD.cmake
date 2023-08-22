# SPDX-FileCopyrightText: 2019 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Universal Scene Description (USD) library
# Find the native USD includes and libraries
# This module defines
#  USD_INCLUDE_DIRS, where to find USD headers, Set when
#                        USD_INCLUDE_DIR is found.
#  USD_LIBRARIES, libraries to link against to use USD.
#  USD_ROOT_DIR, The base directory to search for USD.
#                    This can also be an environment variable.
#  USD_FOUND, If false, do not try to use USD.
#

# If `USD_ROOT_DIR` was defined in the environment, use it.
if(DEFINED USD_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{USD_ROOT_DIR})
  set(USD_ROOT_DIR $ENV{USD_ROOT_DIR})
else()
  set(USD_ROOT_DIR "")
endif()

set(_usd_SEARCH_DIRS
  ${USD_ROOT_DIR}
  /opt/lib/usd
)

find_path(USD_INCLUDE_DIR
  NAMES
    pxr/usd/usd/api.h
  HINTS
    ${_usd_SEARCH_DIRS}
  PATH_SUFFIXES
    include
  DOC "Universal Scene Description (USD) header files"
)

if(NOT DEFINED PXR_LIB_PREFIX)
  set(PXR_LIB_PREFIX "")
endif()

# Since USD 21.11 the libraries are prefixed with "usd_", i.e.
# "libusd_m.a" became "libusd_usd_m.a".
# See https://github.com/PixarAnimationStudios/USD/blob/release/CHANGELOG.md#2111---2021-11-01
find_library(USD_LIBRARY
  NAMES
    usd_usd_m usd_usd_ms usd_m usd_ms
    ${PXR_LIB_PREFIX}usd
  NAMES_PER_DIR
  HINTS
    ${_usd_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib lib/static
  DOC "Universal Scene Description (USD) monolithic library"
)

if(NOT USD_LIBRARY)
  set(USD_FOUND FALSE)
else()
  # handle the QUIETLY and REQUIRED arguments and set USD_FOUND to TRUE if
  # all listed variables are TRUE
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(USD DEFAULT_MSG USD_LIBRARY USD_INCLUDE_DIR)

  if(USD_FOUND)
    get_filename_component(USD_LIBRARY_DIR ${USD_LIBRARY} DIRECTORY)
    set(USD_INCLUDE_DIRS ${USD_INCLUDE_DIR})
    set(USD_LIBRARIES ${USD_LIBRARY})
    if(EXISTS ${USD_INCLUDE_DIR}/pxr/base/tf/pyModule.h)
      set(USD_PYTHON_SUPPORT ON)
    endif()
  endif()
endif()

mark_as_advanced(
  USD_INCLUDE_DIR
  USD_LIBRARY_DIR
  USD_LIBRARY
)

unset(_usd_SEARCH_DIRS)
