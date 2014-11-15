# - Find PugiXML library
# Find the native PugiXML includes and library
# This module defines
#  PUGIXML_INCLUDE_DIRS, where to find pugixml.h, Set when
#                        PugiXML is found.
#  PUGIXML_LIBRARIES, libraries to link against to use PugiiXML.
#  PUGIXML_ROOT_DIR, The base directory to search for PugiXML.
#                    This can also be an environment variable.
#  PUGIXML_FOUND, If false, do not try to use PugiXML.
#
# also defined, but not for general use are
#  PUGIXML_LIBRARY, where to find the PugiXML library.

#=============================================================================
# Copyright 2014 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# If PUGIXML_ROOT_DIR was defined in the environment, use it.
IF(NOT PUGIXML_ROOT_DIR AND NOT $ENV{PUGIXML_ROOT_DIR} STREQUAL "")
  SET(PUGIXML_ROOT_DIR $ENV{PUGIXML_ROOT_DIR})
ENDIF()

SET(_pugixml_SEARCH_DIRS
  ${PUGIXML_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt/lib/oiio
)

FIND_PATH(PUGIXML_INCLUDE_DIR
  NAMES
    pugixml.h
  HINTS
    ${_pugixml_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(PUGIXML_LIBRARY
  NAMES
    pugixml
  HINTS
    ${_pugixml_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set PUGIXML_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PUGIXML DEFAULT_MSG
    PUGIXML_LIBRARY PUGIXML_INCLUDE_DIR)

IF(PUGIXML_FOUND)
  SET(PUGIXML_LIBRARIES ${PUGIXML_LIBRARY})
  SET(PUGIXML_INCLUDE_DIRS ${PUGIXML_INCLUDE_DIR})
ELSE()
  SET(PUGIXML_PUGIXML_FOUND FALSE)
ENDIF()

MARK_AS_ADVANCED(
  PUGIXML_INCLUDE_DIR
  PUGIXML_LIBRARY
)
