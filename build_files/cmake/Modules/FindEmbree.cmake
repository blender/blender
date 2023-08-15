# SPDX-FileCopyrightText: 2018 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Embree library
# Find the native Embree includes and library
# This module defines
#  EMBREE_INCLUDE_DIRS, where to find rtcore.h, Set when
#                            EMBREE_INCLUDE_DIR is found.
#  EMBREE_LIBRARIES, libraries to link against to use Embree.
#  EMBREE_ROOT_DIR, The base directory to search for Embree.
#                        This can also be an environment variable.
#  EMBREEFOUND, If false, do not try to use Embree.

# If `EMBREE_ROOT_DIR` was defined in the environment, use it.
IF(DEFINED EMBREE_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{EMBREE_ROOT_DIR})
  SET(EMBREE_ROOT_DIR $ENV{EMBREE_ROOT_DIR})
ELSE()
  SET(EMBREE_ROOT_DIR "")
ENDIF()

SET(_embree_SEARCH_DIRS
  ${EMBREE_ROOT_DIR}
  /opt/lib/embree
)

FIND_PATH(EMBREE_INCLUDE_DIR
  NAMES
    embree4/rtcore.h
    embree3/rtcore.h
  HINTS
    ${_embree_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

IF(EXISTS ${EMBREE_INCLUDE_DIR}/embree4/rtcore_config.h)
  SET(EMBREE_MAJOR_VERSION 4)
ELSE()
  SET(EMBREE_MAJOR_VERSION 3)
ENDIF()

IF(EMBREE_INCLUDE_DIR)
  FILE(READ ${EMBREE_INCLUDE_DIR}/embree${EMBREE_MAJOR_VERSION}/rtcore_config.h _embree_config_header)
  IF(_embree_config_header MATCHES "#define EMBREE_STATIC_LIB")
    SET(EMBREE_STATIC_LIB TRUE)
  ELSE()
    SET(EMBREE_STATIC_LIB FALSE)
  ENDIF()
  IF(_embree_config_header MATCHES "#define EMBREE_SYCL_SUPPORT")
    SET(EMBREE_SYCL_SUPPORT TRUE)
  ELSE()
    SET(EMBREE_SYCL_SUPPORT FALSE)
  ENDIF()
  UNSET(_embree_config_header)
ENDIF()

IF(EMBREE_STATIC_LIB)
  IF(NOT (("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64") OR (APPLE AND ("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64"))))
    SET(_embree_SIMD_COMPONENTS
      embree_sse42
      embree_avx
      embree_avx2
    )
  ENDIF()

  IF(EMBREE_SYCL_SUPPORT)
    SET(_embree_GPU_COMPONENTS
      embree4_sycl
      embree_rthwif
    )
  ENDIF()

  SET(_embree_FIND_COMPONENTS
    embree${EMBREE_MAJOR_VERSION}
    ${_embree_SIMD_COMPONENTS}
    ${_embree_GPU_COMPONENTS}
    lexers
    math
    simd
    sys
    tasking
  )
ELSE()
  SET(_embree_FIND_COMPONENTS
    embree${EMBREE_MAJOR_VERSION}
  )
  IF(EMBREE_SYCL_SUPPORT)
    LIST(APPEND _embree_FIND_COMPONENTS
      embree4_sycl
    )
  ENDIF()
ENDIF()

SET(_embree_LIBRARIES)
FOREACH(COMPONENT ${_embree_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  FIND_LIBRARY(EMBREE_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_embree_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  LIST(APPEND _embree_LIBRARIES "${EMBREE_${UPPERCOMPONENT}_LIBRARY}")
ENDFOREACH()

# handle the QUIETLY and REQUIRED arguments and set EMBREE_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Embree DEFAULT_MSG
    _embree_LIBRARIES EMBREE_INCLUDE_DIR)

IF(EMBREE_FOUND)
  SET(EMBREE_LIBRARIES ${_embree_LIBRARIES})
  SET(EMBREE_INCLUDE_DIRS ${EMBREE_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  EMBREE_INCLUDE_DIR
  EMBREE_MAJOR_VERSION
  EMBREE_SYCL_SUPPORT
  EMBREE_STATIC_LIB
)

FOREACH(COMPONENT ${_embree_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  MARK_AS_ADVANCED(EMBREE_${UPPERCOMPONENT}_LIBRARY)
ENDFOREACH()

UNSET(_embree_SEARCH_DIRS)
UNSET(_embree_FIND_COMPONENTS)
UNSET(_embree_LIBRARIES)
