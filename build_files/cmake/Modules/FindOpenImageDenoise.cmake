# - Find OpenImageDenoise library
# Find the native OpenImageDenoise includes and library
# This module defines
#  OPENIMAGEDENOISE_INCLUDE_DIRS, where to find oidn.h, Set when
#                    OPENIMAGEDENOISE is found.
#  OPENIMAGEDENOISE_LIBRARIES, libraries to link against to use OpenImageDenoise.
#  OPENIMAGEDENOISE_ROOT_DIR, The base directory to search for OpenImageDenoise.
#                This can also be an environment variable.
#  OPENIMAGEDENOISE_FOUND, If false, do not try to use OpenImageDenoise.
#
# also defined, but not for general use are
#  OPENIMAGEDENOISE_LIBRARY, where to find the OpenImageDenoise library.

#=============================================================================
# Copyright 2019 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# If OPENIMAGEDENOISE_ROOT_DIR was defined in the environment, use it.
IF(NOT OPENIMAGEDENOISE_ROOT_DIR AND NOT $ENV{OPENIMAGEDENOISE_ROOT_DIR} STREQUAL "")
  SET(OPENIMAGEDENOISE_ROOT_DIR $ENV{OPENIMAGEDENOISE_ROOT_DIR})
ENDIF()

SET(_openimagedenoise_SEARCH_DIRS
  ${OPENIMAGEDENOISE_ROOT_DIR}
  /opt/lib/openimagedenoise
)

FIND_PATH(OPENIMAGEDENOISE_INCLUDE_DIR
  NAMES
    OpenImageDenoise/oidn.h
  HINTS
    ${_openimagedenoise_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

SET(_openimagedenoise_FIND_COMPONENTS
  OpenImageDenoise
)

# These are needed when building statically
SET(_openimagedenoise_FIND_STATIC_COMPONENTS
  common

  # These additional library names change between versions, we list all of them
  # so builds work with multiple versions. Missing libraries are skipped.
  dnnl_cpu
  dnnl_common
  dnnl_cpu # Second time because of circular dependency
  mkldnn
  dnnl
)

SET(_openimagedenoise_LIBRARIES)
FOREACH(COMPONENT ${_openimagedenoise_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_openimagedenoise_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  LIST(APPEND _openimagedenoise_LIBRARIES "${OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY}")
ENDFOREACH()

FOREACH(COMPONENT ${_openimagedenoise_FIND_STATIC_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_openimagedenoise_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  MARK_AS_ADVANCED(OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY)
  IF(OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY)
    LIST(APPEND _openimagedenoise_LIBRARIES "${OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY}")
  ENDIF()
ENDFOREACH()

FIND_LIBRARY(OPENIMAGEDENOISE_LIBRARY
  NAMES
    OpenImageDenoise
  HINTS
    ${_openimagedenoise_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set OPENIMAGEDENOISE_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenImageDenoise DEFAULT_MSG
    OPENIMAGEDENOISE_LIBRARY OPENIMAGEDENOISE_INCLUDE_DIR)

IF(OPENIMAGEDENOISE_FOUND)
  SET(OPENIMAGEDENOISE_LIBRARIES ${_openimagedenoise_LIBRARIES})
  SET(OPENIMAGEDENOISE_INCLUDE_DIRS ${OPENIMAGEDENOISE_INCLUDE_DIR})
ELSE()
  SET(OPENIMAGEDENOISE_FOUND FALSE)
ENDIF()

MARK_AS_ADVANCED(
  OPENIMAGEDENOISE_INCLUDE_DIR
)

FOREACH(COMPONENT ${_openimagedenoise_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  MARK_AS_ADVANCED(OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY)
ENDFOREACH()

UNSET(_openimagedenoise_SEARCH_DIRS)
UNSET(_openimagedenoise_FIND_COMPONENTS)
UNSET(_openimagedenoise_LIBRARIES)
