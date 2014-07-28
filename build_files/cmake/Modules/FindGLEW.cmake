# - Find GLEW library
# Find the native Glew includes and library
# This module defines
#  GLEW_INCLUDE_DIRS, where to find glew.h, Set when
#                        GLEW_INCLUDE_DIR is found.
#  GLEW_ROOT_DIR, The base directory to search for Glew.
#                    This can also be an environment variable.
#  GLEW_FOUND, If false, do not try to use Glew.
#
# also defined,
#  GLEW_LIBRARY, where to find the Glew library.
#  GLEW_MX_LIBRARY, where to find the GlewMX library.

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

# If GLEW_ROOT_DIR was defined in the environment, use it.
IF(NOT GLEW_ROOT_DIR AND NOT $ENV{GLEW_ROOT_DIR} STREQUAL "")
	SET(GLEW_ROOT_DIR $ENV{GLEW_ROOT_DIR})
ENDIF()

SET(_glew_SEARCH_DIRS
  ${GLEW_ROOT_DIR}
  /usr/local
)

FIND_PATH(GLEW_INCLUDE_DIR
  NAMES
    GL/glew.h
  HINTS
    ${_glew_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(GLEW_LIBRARY
  NAMES
    GLEW
  HINTS
    ${_glew_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )


FIND_LIBRARY(GLEW_MX_LIBRARY
  NAMES
    GLEWmx
  HINTS
    ${_glew_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set GLEW_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Glew DEFAULT_MSG
    GLEW_LIBRARY GLEW_INCLUDE_DIR)

IF(GLEW_FOUND)
  SET(GLEW_INCLUDE_DIRS ${GLEW_INCLUDE_DIR})
ENDIF(GLEW_FOUND)

MARK_AS_ADVANCED(
  GLEW_INCLUDE_DIR
  GLEW_LIBRARY
  GLEW_MX_LIBRARY
)
