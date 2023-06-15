# SPDX-FileCopyrightText: 2022 Blender Foundation
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

# If WEBP_ROOT_DIR was defined in the environment, use it.
IF(NOT WEBP_ROOT_DIR AND NOT $ENV{WEBP_ROOT_DIR} STREQUAL "")
  SET(WEBP_ROOT_DIR $ENV{WEBP_ROOT_DIR})
ENDIF()

SET(_webp_SEARCH_DIRS
  ${WEBP_ROOT_DIR}
  /opt/lib/webp
)

FIND_PATH(WEBP_INCLUDE_DIR
  NAMES
    webp/types.h
  HINTS
    ${_webp_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

SET(_webp_FIND_COMPONENTS
  webp
  webpmux
  webpdemux
)

SET(_webp_LIBRARIES)
FOREACH(COMPONENT ${_webp_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(WEBP_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    NAMES_PER_DIR
    HINTS
      ${_webp_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib lib/static
    )
  LIST(APPEND _webp_LIBRARIES "${WEBP_${UPPERCOMPONENT}_LIBRARY}")
ENDFOREACH()

IF(${WEBP_WEBP_LIBRARY_NOTFOUND})
  set(WEBP_FOUND FALSE)
ELSE()
  # handle the QUIETLY and REQUIRED arguments and set WEBP_FOUND to TRUE if
  # all listed variables are TRUE
  INCLUDE(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(WebP DEFAULT_MSG _webp_LIBRARIES WEBP_INCLUDE_DIR)

  IF(WEBP_FOUND)
    get_filename_component(WEBP_LIBRARY_DIR ${WEBP_WEBP_LIBRARY} DIRECTORY)
    SET(WEBP_INCLUDE_DIRS ${WEBP_INCLUDE_DIR})
    SET(WEBP_LIBRARIES ${_webp_LIBRARIES})
  ELSE()
    SET(WEBPL_PUGIXML_FOUND FALSE)
  ENDIF()
ENDIF()

MARK_AS_ADVANCED(
  WEBP_INCLUDE_DIR
  WEBP_LIBRARY_DIR

  # Generated names.
  WEBP_WEBPDEMUX_LIBRARY
  WEBP_WEBPMUX_LIBRARY
  WEBP_WEBP_LIBRARY
)
