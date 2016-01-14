# - Find OpenSubdiv library
# Find the native OpenSubdiv includes and library
# This module defines
#  OPENSUBDIV_INCLUDE_DIRS, where to find OpenSubdiv headers, Set when
#                           OPENSUBDIV_INCLUDE_DIR is found.
#  OPENSUBDIV_LIBRARIES, libraries to link against to use OpenSubdiv.
#  OPENSUBDIV_ROOT_DIR, the base directory to search for OpenSubdiv.
#                        This can also be an environment variable.
#  OPENSUBDIV_FOUND, if false, do not try to use OpenSubdiv.
#
# also defined, but not for general use are
#  OPENSUBDIV_LIBRARY, where to find the OpenSubdiv library.

#=============================================================================
# Copyright 2013 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# If OPENSUBDIV_ROOT_DIR was defined in the environment, use it.
IF(NOT OPENSUBDIV_ROOT_DIR AND NOT $ENV{OPENSUBDIV_ROOT_DIR} STREQUAL "")
  SET(OPENSUBDIV_ROOT_DIR $ENV{OPENSUBDIV_ROOT_DIR})
ENDIF()

SET(_opensubdiv_FIND_COMPONENTS
  osdGPU
  osdCPU
)

SET(_opensubdiv_SEARCH_DIRS
  ${OPENSUBDIV_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt/lib/opensubdiv
  /opt/lib/osd # install_deps.sh
)

FIND_PATH(OPENSUBDIV_INCLUDE_DIR
  NAMES
    opensubdiv/osd/mesh.h
  HINTS
    ${_opensubdiv_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

SET(_opensubdiv_LIBRARIES)
FOREACH(COMPONENT ${_opensubdiv_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(OPENSUBDIV_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_opensubdiv_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  LIST(APPEND _opensubdiv_LIBRARIES "${OPENSUBDIV_${UPPERCOMPONENT}_LIBRARY}")
ENDFOREACH()

MACRO(OPENSUBDIV_CHECK_CONTROLLER
      controller_include_file
      variable_name)
  IF(EXISTS "${OPENSUBDIV_INCLUDE_DIR}/opensubdiv/osd/${controller_include_file}")
    SET(${variable_name} TRUE)
  ELSE()
    SET(${variable_name} FALSE)
  ENDIF()
ENDMACRO()


# handle the QUIETLY and REQUIRED arguments and set OPENSUBDIV_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenSubdiv DEFAULT_MSG
    _opensubdiv_LIBRARIES OPENSUBDIV_INCLUDE_DIR)

IF(OPENSUBDIV_FOUND)
  SET(OPENSUBDIV_LIBRARIES ${_opensubdiv_LIBRARIES})
  SET(OPENSUBDIV_INCLUDE_DIRS ${OPENSUBDIV_INCLUDE_DIR})

  # Find available compute controllers.

  FIND_PACKAGE(OpenMP)
  IF(OPENMP_FOUND)
    SET(OPENSUBDIV_HAS_OPENMP TRUE)
  ELSE()
    SET(OPENSUBDIV_HAS_OPENMP FALSE)
  ENDIF()

  OPENSUBDIV_CHECK_CONTROLLER("tbbEvaluator.h" OPENSUBDIV_HAS_TBB)
  OPENSUBDIV_CHECK_CONTROLLER("clEvaluator.h" OPENSUBDIV_HAS_OPENCL)
  OPENSUBDIV_CHECK_CONTROLLER("cudaEvaluator.h" OPENSUBDIV_HAS_CUDA)
  OPENSUBDIV_CHECK_CONTROLLER("glXFBEvaluator.h" OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK)
  OPENSUBDIV_CHECK_CONTROLLER("glComputeEvaluator.h" OPENSUBDIV_HAS_GLSL_COMPUTE)
ENDIF(OPENSUBDIV_FOUND)

MARK_AS_ADVANCED(
  OPENSUBDIV_INCLUDE_DIR
)
FOREACH(COMPONENT ${_opensubdiv_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  MARK_AS_ADVANCED(OPENSUBDIV_${UPPERCOMPONENT}_LIBRARY)
ENDFOREACH()
