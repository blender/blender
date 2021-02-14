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
# Distributed under the OSI-approved BSD 3-Clause License,
# see accompanying file BSD-3-Clause-license.txt for details.
#=============================================================================

# If OPENCOLORIO_ROOT_DIR was defined in the environment, use it.
IF(NOT OPENCOLORIO_ROOT_DIR AND NOT $ENV{OPENCOLORIO_ROOT_DIR} STREQUAL "")
  SET(OPENCOLORIO_ROOT_DIR $ENV{OPENCOLORIO_ROOT_DIR})
ENDIF()

SET(_opencolorio_FIND_COMPONENTS
  OpenColorIO
  yaml-cpp
  expat
  pystring
)

SET(_opencolorio_SEARCH_DIRS
  ${OPENCOLORIO_ROOT_DIR}
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
      lib64 lib lib64/static lib/static
    )
  IF(OPENCOLORIO_${UPPERCOMPONENT}_LIBRARY)
    LIST(APPEND _opencolorio_LIBRARIES "${OPENCOLORIO_${UPPERCOMPONENT}_LIBRARY}")
  ENDIF()
ENDFOREACH()

IF(EXISTS "${OPENCOLORIO_INCLUDE_DIR}/OpenColorIO/OpenColorABI.h")
  # Search twice, because this symbol changed between OCIO 1.x and 2.x
  FILE(STRINGS "${OPENCOLORIO_INCLUDE_DIR}/OpenColorIO/OpenColorABI.h" _opencolorio_version
    REGEX "^#define OCIO_VERSION_STR[ \t].*$")
  IF(NOT _opencolorio_version)
    file(STRINGS "${OPENCOLORIO_INCLUDE_DIR}/OpenColorIO/OpenColorABI.h" _opencolorio_version
      REGEX "^#define OCIO_VERSION[ \t].*$")
  ENDIF()
  STRING(REGEX MATCHALL "[0-9]+[.0-9]+" OPENCOLORIO_VERSION ${_opencolorio_version})
ENDIF()

# handle the QUIETLY and REQUIRED arguments and set OPENCOLORIO_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenColorIO
    REQUIRED_VARS _opencolorio_LIBRARIES OPENCOLORIO_INCLUDE_DIR
    VERSION_VAR OPENCOLORIO_VERSION)

IF(OPENCOLORIO_FOUND)
  SET(OPENCOLORIO_LIBRARIES ${_opencolorio_LIBRARIES})
  SET(OPENCOLORIO_INCLUDE_DIRS ${OPENCOLORIO_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  OPENCOLORIO_INCLUDE_DIR
  OPENCOLORIO_LIBRARY
  OPENCOLORIO_OPENCOLORIO_LIBRARY
  OPENCOLORIO_TINYXML_LIBRARY
  OPENCOLORIO_YAML-CPP_LIBRARY
  OPENCOLORIO_VERSION
)

UNSET(COMPONENT)
UNSET(UPPERCOMPONENT)
UNSET(_opencolorio_FIND_COMPONENTS)
UNSET(_opencolorio_LIBRARIES)
UNSET(_opencolorio_SEARCH_DIRS)
