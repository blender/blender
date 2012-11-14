# - Find OpenColorIO library
# Find the native OpenColorIO includes and library
# This module defines
#  OPENCOLORIO_INCLUDE_DIRS, where to find OpenColorIO.h, Set when
#                            OPENCOLORIO_INCLUDE_DIR is found.
#  OPENCOLORIO_LIBRARIES, libraries to link against to use OpenColorIO.
#  OPENCOLORIO_ROOT_DIR, The base directory to search for OpenColorIO.
#                        This can also be an environment variable.
#  OPENCOLORIO_FOUND, If false, do not try to use OpenColorIO.
#
# also defined, but not for general use are
#  OPENCOLORIO_LIBRARY, where to find the OpenColorIO library.

#=============================================================================
# Copyright 2012 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# If OPENCOLORIO_ROOT_DIR was defined in the environment, use it.
IF(NOT OPENCOLORIO_ROOT_DIR AND NOT $ENV{OPENCOLORIO_ROOT_DIR} STREQUAL "")
  SET(OPENCOLORIO_ROOT_DIR $ENV{OPENCOLORIO_ROOT_DIR})
ENDIF()

SET(_opencolorio_FIND_COMPONENTS
  OpenColorIO
  yaml-cpp
  tinyxml
)

SET(_opencolorio_SEARCH_DIRS
  ${OPENCOLORIO_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt/lib/ocio
)

FIND_PATH(OPENCOLORIO_INCLUDE_DIR
  NAMES
    OpenColorIO/OpenColorIO.h
  HINTS
    ${_opencolorio_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

SET(_opencolorio_LIBRARIES)
FOREACH(COMPONENT ${_opencolorio_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(OPENCOLORIO_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_opencolorio_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  if(OPENCOLORIO_${UPPERCOMPONENT}_LIBRARY)
    LIST(APPEND _opencolorio_LIBRARIES "${OPENCOLORIO_${UPPERCOMPONENT}_LIBRARY}")
  endif()
ENDFOREACH()

# handle the QUIETLY and REQUIRED arguments and set OPENCOLORIO_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenColorIO DEFAULT_MSG
    _opencolorio_LIBRARIES OPENCOLORIO_INCLUDE_DIR)

IF(OPENCOLORIO_FOUND)
  SET(OPENCOLORIO_LIBRARIES ${_opencolorio_LIBRARIES})
  SET(OPENCOLORIO_INCLUDE_DIRS ${OPENCOLORIO_INCLUDE_DIR})
ENDIF(OPENCOLORIO_FOUND)

MARK_AS_ADVANCED(
  OPENCOLORIO_INCLUDE_DIR
  OPENCOLORIO_LIBRARY
)

