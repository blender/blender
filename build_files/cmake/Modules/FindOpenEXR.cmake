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
# Distributed under the OSI-approved BSD 3-Clause License,
# see accompanying file BSD-3-Clause-license.txt for details.
#=============================================================================

# If OPENEXR_ROOT_DIR was defined in the environment, use it.
IF(NOT OPENEXR_ROOT_DIR AND NOT $ENV{OPENEXR_ROOT_DIR} STREQUAL "")
  SET(OPENEXR_ROOT_DIR $ENV{OPENEXR_ROOT_DIR})
ENDIF()

# Old versions (before 2.0?) do not have any version string, just assuming this should be fine though.
SET(_openexr_libs_ver_init "2.0")

SET(_openexr_SEARCH_DIRS
  ${OPENEXR_ROOT_DIR}
  /opt/lib/openexr
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

# Different library names in 3.0, and Imath and Half moved out.
IF(OPENEXR_VERSION VERSION_GREATER_EQUAL "3.0.0")
  SET(_openexr_FIND_COMPONENTS
    Iex
    IlmThread
    OpenEXR
    OpenEXRCore
  )
ELSE()
  SET(_openexr_FIND_COMPONENTS
    Half
    Iex
    IlmImf
    IlmThread
    Imath
  )
ENDIF()

SET(_openexr_LIBRARIES)
FOREACH(COMPONENT ${_openexr_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(OPENEXR_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}-${_openexr_libs_ver} ${COMPONENT}
    NAMES_PER_DIR
    HINTS
      ${_openexr_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  LIST(APPEND _openexr_LIBRARIES "${OPENEXR_${UPPERCOMPONENT}_LIBRARY}")
ENDFOREACH()

UNSET(_openexr_libs_ver)

IF(OPENEXR_VERSION VERSION_GREATER_EQUAL "3.0.0")
  # For OpenEXR 3.x, we also need to find the now separate Imath library.
  # For simplicity we add it to the OpenEXR includes and libraries, as we
  # have no direct dependency on Imath and it's simpler to support both
  # 2.x and 3.x this way.

  # Find include directory
  FIND_PATH(IMATH_INCLUDE_DIR
    NAMES
      Imath/ImathMath.h
    HINTS
      ${_openexr_SEARCH_DIRS}
    PATH_SUFFIXES
      include
  )

  # Find version
  FIND_FILE(_imath_config
    NAMES
      ImathConfig.h
    PATHS
      ${IMATH_INCLUDE_DIR}/Imath
    NO_DEFAULT_PATH
  )

  # Find line with version, extract string, and format for library suffix.
  FILE(STRINGS "${_imath_config}" _imath_build_specification
       REGEX "^[ \t]*#define[ \t]+IMATH_VERSION_STRING[ \t]+\"[.0-9]+\".*$")
  STRING(REGEX REPLACE ".*#define[ \t]+IMATH_VERSION_STRING[ \t]+\"([.0-9]+)\".*"
         "\\1" _imath_libs_ver ${_imath_build_specification})
  STRING(REGEX REPLACE "([0-9]+)[.]([0-9]+).*" "\\1_\\2" _imath_libs_ver ${_imath_libs_ver})

  # Find library, with or without version number.
  FIND_LIBRARY(IMATH_LIBRARY
    NAMES
      Imath-${_imath_libs_ver} Imath
    NAMES_PER_DIR
    HINTS
      ${_openexr_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  LIST(APPEND _openexr_LIBRARIES "${IMATH_LIBRARY}")

  # In cmake version 3.21 and up, we can instead use the NO_CACHE option for
  # FIND_FILE so we don't need to clear it from the cache here.
  UNSET(_imath_config CACHE)
  UNSET(_imath_libs_ver)
  UNSET(_imath_build_specification)
ENDIF()

# handle the QUIETLY and REQUIRED arguments and set OPENEXR_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenEXR  DEFAULT_MSG
    _openexr_LIBRARIES OPENEXR_INCLUDE_DIR)

IF(OPENEXR_FOUND)
  SET(OPENEXR_LIBRARIES ${_openexr_LIBRARIES})
  # Both include paths are needed because of dummy OSL headers mixing
  # #include <OpenEXR/foo.h> and #include <foo.h>, as well as Alembic
  # include <half.h> directly.
  SET(OPENEXR_INCLUDE_DIRS
    ${OPENEXR_INCLUDE_DIR}
    ${OPENEXR_INCLUDE_DIR}/OpenEXR)

  IF(OPENEXR_VERSION VERSION_GREATER_EQUAL "3.0.0")
    LIST(APPEND OPENEXR_INCLUDE_DIRS
      ${IMATH_INCLUDE_DIR}
      ${IMATH_INCLUDE_DIR}/Imath)
  ENDIF()
ENDIF()

MARK_AS_ADVANCED(
  OPENEXR_INCLUDE_DIR
  OPENEXR_VERSION
  IMATH_INCLUDE_DIR
  IMATH_LIBRARY
)
FOREACH(COMPONENT ${_openexr_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  MARK_AS_ADVANCED(OPENEXR_${UPPERCOMPONENT}_LIBRARY)
ENDFOREACH()

UNSET(COMPONENT)
UNSET(UPPERCOMPONENT)
UNSET(_openexr_FIND_COMPONENTS)
UNSET(_openexr_LIBRARIES)
UNSET(_openexr_SEARCH_DIRS)
