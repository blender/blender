# - Find OpenShadingLanguage library
# Find the native OpenShadingLanguage includes and library
# This module defines
#  OSL_INCLUDE_DIRS, where to find OSL headers, Set when
#                    OSL_INCLUDE_DIR is found.
#  OSL_LIBRARIES, libraries to link against to use OSL.
#  OSL_ROOT_DIR, the base directory to search for OSL.
#                This can also be an environment variable.
#  OSL_COMPILER, full path to OSL script compiler.
#  OSL_FOUND, if false, do not try to use OSL.
#  OSL_LIBRARY_VERSION_MAJOR, OSL_LIBRARY_VERSION_MINOR,  the major
#                and minor versions of OSL library if found.
#
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

# If OSL_ROOT_DIR was defined in the environment, use it.
IF(NOT OSL_ROOT_DIR AND NOT $ENV{OSL_ROOT_DIR} STREQUAL "")
  SET(OSL_ROOT_DIR $ENV{OSL_ROOT_DIR})
ENDIF()

SET(_osl_FIND_COMPONENTS
  oslcomp
  oslexec
  oslquery
)

SET(_osl_SEARCH_DIRS
  ${OSL_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt/lib/osl
)

FIND_PATH(OSL_INCLUDE_DIR
  NAMES
    OSL/oslversion.h
  HINTS
    ${_osl_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

SET(_osl_LIBRARIES)
FOREACH(COMPONENT ${_osl_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(OSL_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_osl_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  LIST(APPEND _osl_LIBRARIES "${OSL_${UPPERCOMPONENT}_LIBRARY}")
ENDFOREACH()

FIND_PROGRAM(OSL_COMPILER oslc
             HINTS ${_osl_SEARCH_DIRS}
             PATH_SUFFIXES bin)

# handle the QUIETLY and REQUIRED arguments and set OSL_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OSL DEFAULT_MSG _osl_LIBRARIES OSL_INCLUDE_DIR OSL_COMPILER)

IF(OSL_FOUND)
  SET(OSL_LIBRARIES ${_osl_LIBRARIES})
  SET(OSL_INCLUDE_DIRS ${OSL_INCLUDE_DIR})

  FILE(STRINGS "${OSL_INCLUDE_DIR}/OSL/oslversion.h" OSL_LIBRARY_VERSION_MAJOR
       REGEX "^[ \t]*#define[ \t]+OSL_LIBRARY_VERSION_MAJOR[ \t]+[0-9]+.*$")
  FILE(STRINGS "${OSL_INCLUDE_DIR}/OSL/oslversion.h" OSL_LIBRARY_VERSION_MINOR
       REGEX "^[ \t]*#define[ \t]+OSL_LIBRARY_VERSION_MINOR[ \t]+[0-9]+.*$")
  STRING(REGEX REPLACE ".*#define[ \t]+OSL_LIBRARY_VERSION_MAJOR[ \t]+([.0-9]+).*"
         "\\1" OSL_LIBRARY_VERSION_MAJOR ${OSL_LIBRARY_VERSION_MAJOR})
  STRING(REGEX REPLACE ".*#define[ \t]+OSL_LIBRARY_VERSION_MINOR[ \t]+([.0-9]+).*"
         "\\1" OSL_LIBRARY_VERSION_MINOR ${OSL_LIBRARY_VERSION_MINOR})
ENDIF(OSL_FOUND)

MARK_AS_ADVANCED(
  OSL_INCLUDE_DIR
)
FOREACH(COMPONENT ${_osl_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  MARK_AS_ADVANCED(OSL_${UPPERCOMPONENT}_LIBRARY)
ENDFOREACH()
