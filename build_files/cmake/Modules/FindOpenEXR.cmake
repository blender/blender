# - Find OpenEXR library
# Find the native OpenEXR includes and library
# This module defines
#  OPENEXR_INCLUDE_DIRS, where to find ImfXdr.h, etc. Set when
#                        OPENEXR_INCLUDE_DIR is found.
#  OPENEXR_LIBRARIES, libraries to link against to use OpenEXR.
#  OPENEXR_ROOT_DIR, The base directory to search for OpenEXR.
#                    This can also be an environment variable.
#  OPENEXR_FOUND, If false, do not try to use OpenEXR.
#
# For individual library access these advanced settings are available
#  OPENEXR_HALF_LIBRARY, Path to Half library
#  OPENEXR_IEX_LIBRARY, Path to Half library
#  OPENEXR_ILMIMF_LIBRARY, Path to Ilmimf library
#  OPENEXR_ILMTHREAD_LIBRARY, Path to IlmThread library
#  OPENEXR_IMATH_LIBRARY, Path to Imath library
#
# also defined, but not for general use are
#  OPENEXR_LIBRARY, where to find the OpenEXR library.

#=============================================================================
# Copyright 2011 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# If OPENEXR_ROOT_DIR was defined in the environment, use it.
IF(NOT OPENEXR_ROOT_DIR AND NOT $ENV{OPENEXR_ROOT_DIR} STREQUAL "")
  SET(OPENEXR_ROOT_DIR $ENV{OPENEXR_ROOT_DIR})
ENDIF()

# Old versions (before 2.0?) do not have any version string, just assuming this should be fine though.
SET(_openexr_libs_ver_init "2.0")

SET(_openexr_FIND_COMPONENTS
  Half
  Iex
  IlmImf
  IlmThread
  Imath
)

SET(_openexr_SEARCH_DIRS
  ${OPENEXR_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
)

FIND_PATH(OPENEXR_INCLUDE_DIR
  NAMES
    OpenEXR/ImfXdr.h
  HINTS
    ${_openexr_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

# If the headers were found, get the version from config file, if not already set.
IF(OPENEXR_INCLUDE_DIR)
  IF(NOT OPENEXR_VERSION)

    FIND_FILE(_openexr_CONFIG
      NAMES
        OpenEXRConfig.h
      PATHS
        "${OPENEXR_INCLUDE_DIR}"
        "${OPENEXR_INCLUDE_DIR}/OpenEXR"
      NO_DEFAULT_PATH
    )

    IF(_openexr_CONFIG)
      FILE(STRINGS "${_openexr_CONFIG}" OPENEXR_BUILD_SPECIFICATION
           REGEX "^[ \t]*#define[ \t]+OPENEXR_VERSION_STRING[ \t]+\"[.0-9]+\".*$")
    ELSE()
      MESSAGE(WARNING "Could not find \"OpenEXRConfig.h\" in \"${OPENEXR_INCLUDE_DIR}\"")
    ENDIF()

    IF(OPENEXR_BUILD_SPECIFICATION)
      MESSAGE(STATUS "${OPENEXR_BUILD_SPECIFICATION}")
      STRING(REGEX REPLACE ".*#define[ \t]+OPENEXR_VERSION_STRING[ \t]+\"([.0-9]+)\".*"
             "\\1" _openexr_libs_ver_init ${OPENEXR_BUILD_SPECIFICATION})
    ELSE()
      MESSAGE(WARNING "Could not determine ILMBase library version, assuming ${_openexr_libs_ver_init}.")
    ENDIF()

    UNSET(_openexr_CONFIG CACHE)

  ENDIF()
ENDIF()

SET("OPENEXR_VERSION" ${_openexr_libs_ver_init} CACHE STRING "Version of OpenEXR lib")
UNSET(_openexr_libs_ver_init)

STRING(REGEX REPLACE "([0-9]+)[.]([0-9]+).*" "\\1_\\2" _openexr_libs_ver ${OPENEXR_VERSION})

SET(_openexr_LIBRARIES)
FOREACH(COMPONENT ${_openexr_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(OPENEXR_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}-${_openexr_libs_ver} ${COMPONENT} 
    HINTS
      ${_openexr_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  LIST(APPEND _openexr_LIBRARIES "${OPENEXR_${UPPERCOMPONENT}_LIBRARY}")
ENDFOREACH()

UNSET(_openexr_libs_ver)

# handle the QUIETLY and REQUIRED arguments and set OPENEXR_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenEXR  DEFAULT_MSG
    _openexr_LIBRARIES OPENEXR_INCLUDE_DIR)

IF(OPENEXR_FOUND)
  SET(OPENEXR_LIBRARIES ${_openexr_LIBRARIES})
  # Both include paths are needed because of dummy OSL headers mixing #include <OpenEXR/foo.h> and #include <foo.h> :(
  SET(OPENEXR_INCLUDE_DIRS ${OPENEXR_INCLUDE_DIR} ${OPENEXR_INCLUDE_DIR}/OpenEXR)
ENDIF()

MARK_AS_ADVANCED(
  OPENEXR_INCLUDE_DIR
  OPENEXR_VERSION
)
FOREACH(COMPONENT ${_openexr_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  MARK_AS_ADVANCED(OPENEXR_${UPPERCOMPONENT}_LIBRARY)
ENDFOREACH()
