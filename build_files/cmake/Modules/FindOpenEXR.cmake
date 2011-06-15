# - Find OpenEXR library (copied from FindTIFF.cmake, v 2.8.5)
# Find the native OpenEXR includes and library
# This module defines
#  OPENEXR_INCLUDE_DIRS, where to find ImfXdr.h, etc. Set when
#                        OPENEXR_INCLUDE_DIR is found.
#  OPENEXR_LIBRARIES, libraries to link against to use OpenEXR.
#  OPENEXR_ROOT_DIR, The base directory to search for OpenEXR.
#                    This can also be an environment variable.
#  OPENEXR_FOUND, If false, do not try to use OpenEXR.
# also defined, but not for general use are
#  OPENEXR_LIBRARY, where to find the OpenEXR library.

#=============================================================================
# Copyright 2002-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

# If OPENEXR_ROOT_DIR was defined in the environment, use it.
IF(NOT OPENEXR_ROOT_DIR AND NOT $ENV{OPENEXR_ROOT_DIR} STREQUAL "")
  SET(OPENEXR_ROOT_DIR $ENV{OPENEXR_ROOT_DIR})
ENDIF()

SET(_openexr_FIND_COMPONENTS
  Half
  IlmImf
  Iex
  Imath
)

SET(_openexr_SEARCH_DIRS
  ${OPENEXR_ROOT_DIR}
  /usr/local
  /opt/csw
)

FIND_PATH(OPENEXR_INCLUDE_DIR ImfXdr.h
  HINTS
    ${_openexr_SEARCH_DIRS}
  PATH_SUFFIXES
    include/OpenEXR
)

SET(_openexr_LIBRARIES)
FOREACH(COMPONENT ${_openexr_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(OPENEXR_${UPPERCOMPONENT}_LIBRARY NAMES ${COMPONENT}
      HINTS ${_openexr_SEARCH_DIRS}
      PATH_SUFFIXES lib
      )
  LIST(APPEND _openexr_LIBRARIES "${OPENEXR_${UPPERCOMPONENT}_LIBRARY}")
ENDFOREACH()

# handle the QUIETLY and REQUIRED arguments and set OPENEXR_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenEXR  DEFAULT_MSG
    ${_openexr_LIBRARIES} OPENEXR_INCLUDE_DIR)

IF(OPENEXR_FOUND)
  SET(OPENEXR_LIBRARIES ${_openexr_LIBRARIES})
  SET(OPENEXR_INCLUDE_DIRS ${OPENEXR_INCLUDE_DIR})
ENDIF(OPENEXR_FOUND)

MARK_AS_ADVANCED(
  ${_openexr_LIBRARIES}
  OPENEXR_INCLUDE_DIR
)
