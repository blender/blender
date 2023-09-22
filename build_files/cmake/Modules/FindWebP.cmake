# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find WebP library
# Find the native WebP includes and library
# This module defines
#  WEBP_INCLUDE_DIRS, where to find WebP headers, Set when WebP is found.
#  WEBP_LIBRARIES, libraries to link against to use WebP.
#  WEBP_ROOT_DIR, The base directory to search for WebP.
#                 This can also be an environment variable.
#  WEBP_FOUND, If false, do not try to use WebP.
#
# also defined, but not for general use are
#  WEBP_LIBRARY, where to find the WEBP library.

# If `WEBP_ROOT_DIR` was defined in the environment, use it.
if(DEFINED WEBP_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{WEBP_ROOT_DIR})
  set(WEBP_ROOT_DIR $ENV{WEBP_ROOT_DIR})
else()
  set(WEBP_ROOT_DIR "")
endif()

set(_webp_SEARCH_DIRS
  ${WEBP_ROOT_DIR}
  /opt/lib/webp
)

find_path(WEBP_INCLUDE_DIR
  NAMES
    webp/types.h
  HINTS
    ${_webp_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

set(_webp_FIND_COMPONENTS
  webp
  webpmux
  webpdemux
  sharpyuv # New in 1.3
)

set(_webp_LIBRARIES)
foreach(COMPONENT ${_webp_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  find_library(WEBP_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    NAMES_PER_DIR
    HINTS
      ${_webp_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib lib/static
    )
  if (WEBP_${UPPERCOMPONENT}_LIBRARY)
    list(APPEND _webp_LIBRARIES "${WEBP_${UPPERCOMPONENT}_LIBRARY}")
  endif()
endforeach()

if(NOT WEBP_WEBP_LIBRARY)
  set(WEBP_FOUND FALSE)
else()
  # handle the QUIETLY and REQUIRED arguments and set WEBP_FOUND to TRUE if
  # all listed variables are TRUE
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(WebP DEFAULT_MSG _webp_LIBRARIES WEBP_INCLUDE_DIR)

  if(WEBP_FOUND)
    get_filename_component(WEBP_LIBRARY_DIR ${WEBP_WEBP_LIBRARY} DIRECTORY)
    set(WEBP_INCLUDE_DIRS ${WEBP_INCLUDE_DIR})
    set(WEBP_LIBRARIES ${_webp_LIBRARIES})
  else()
    set(WEBPL_PUGIXML_FOUND FALSE)
  endif()
endif()

mark_as_advanced(
  WEBP_INCLUDE_DIR
  WEBP_LIBRARY_DIR

  # Generated names.
  WEBP_WEBPDEMUX_LIBRARY
  WEBP_WEBPMUX_LIBRARY
  WEBP_WEBP_LIBRARY
  WEBP_SHARPYUV_LIBRARY
)
