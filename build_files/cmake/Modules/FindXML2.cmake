# SPDX-FileCopyrightText: 2011 Blender Foundation
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find XML2 library
# Find the native XML2 includes and library
# This module defines
#  XML2_INCLUDE_DIRS, where to find xml2.h, Set when
#                     XML2_INCLUDE_DIR is found.
#  XML2_LIBRARIES, libraries to link against to use XML2.
#  XML2_ROOT_DIR, The base directory to search for XML2.
#                 This can also be an environment variable.
#  XML2_FOUND, If false, do not try to use XML2.
#
# also defined, but not for general use are
#  XML2_LIBRARY, where to find the XML2 library.

# If `XML2_ROOT_DIR` was defined in the environment, use it.
IF(DEFINED XML2_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{XML2_ROOT_DIR})
  SET(XML2_ROOT_DIR $ENV{XML2_ROOT_DIR})
ELSE()
  SET(XML2_ROOT_DIR "")
ENDIF()

SET(_xml2_SEARCH_DIRS
  ${XML2_ROOT_DIR}
)

FIND_PATH(XML2_INCLUDE_DIR libxml2/libxml/xpath.h
  HINTS
    ${_xml2_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(XML2_LIBRARY
  NAMES
    xml2
  HINTS
    ${_xml2_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set XML2_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(XML2 DEFAULT_MSG
    XML2_LIBRARY XML2_INCLUDE_DIR)

IF(XML2_FOUND)
  SET(XML2_LIBRARIES ${XML2_LIBRARY})
  SET(XML2_INCLUDE_DIRS ${XML2_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  XML2_INCLUDE_DIR
  XML2_LIBRARY
)
