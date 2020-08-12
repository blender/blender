# - Find Embree library
# Find the native Embree includes and library
# This module defines
#  EMBREE_INCLUDE_DIRS, where to find rtcore.h, Set when
#                            EMBREE_INCLUDE_DIR is found.
#  EMBREE_LIBRARIES, libraries to link against to use Embree.
#  EMBREE_ROOT_DIR, The base directory to search for Embree.
#                        This can also be an environment variable.
#  EMBREEFOUND, If false, do not try to use Embree.

#=============================================================================
# Copyright 2018 Blender Foundation.
#
# Distributed under the OSI-approved BSD 3-Clause License,
# see accompanying file BSD-3-Clause-license.txt for details.
#=============================================================================

# If EMBREE_ROOT_DIR was defined in the environment, use it.
IF(NOT EMBREE_ROOT_DIR AND NOT $ENV{EMBREE_ROOT_DIR} STREQUAL "")
  SET(EMBREE_ROOT_DIR $ENV{EMBREE_ROOT_DIR})
ENDIF()

SET(_embree_SEARCH_DIRS
  ${EMBREE_ROOT_DIR}
  /opt/lib/embree
)

FIND_PATH(EMBREE_INCLUDE_DIR
  NAMES
    embree3/rtcore.h
  HINTS
    ${_embree_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)


SET(_embree_FIND_COMPONENTS
  embree3
  embree_avx
  embree_avx2
  embree_sse42
  lexers
  math
  simd
  sys
  tasking
)

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
ENDIF(EMBREE_FOUND)

MARK_AS_ADVANCED(
  EMBREE_INCLUDE_DIR
)

FOREACH(COMPONENT ${_embree_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  MARK_AS_ADVANCED(EMBREE_${UPPERCOMPONENT}_LIBRARY)
ENDFOREACH()

UNSET(_embree_SEARCH_DIRS)
UNSET(_embree_FIND_COMPONENTS)
UNSET(_embree_LIBRARIES)
