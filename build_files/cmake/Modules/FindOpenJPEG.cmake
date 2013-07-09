# - Find OpenJPEG library
# Find the native OpenJPEG includes and library
# This module defines
#  OPENJPEG_INCLUDE_DIRS, where to find openjpeg.h, Set when
#                        OPENJPEG_INCLUDE_DIR is found.
#  OPENJPEG_LIBRARIES, libraries to link against to use OpenJPEG.
#  OPENJPEG_ROOT_DIR, The base directory to search for OpenJPEG.
#                    This can also be an environment variable.
#  OPENJPEG_FOUND, If false, do not try to use OpenJPEG.
#
# also defined, but not for general use are
#  OPENJPEG_LIBRARY, where to find the OpenJPEG library.

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

# If OPENJPEG_ROOT_DIR was defined in the environment, use it.
IF(NOT OPENJPEG_ROOT_DIR AND NOT $ENV{OPENJPEG_ROOT_DIR} STREQUAL "")
  SET(OPENJPEG_ROOT_DIR $ENV{OPENJPEG_ROOT_DIR})
ENDIF()

SET(_openjpeg_SEARCH_DIRS
  ${OPENJPEG_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
)

FIND_PATH(OPENJPEG_INCLUDE_DIR
  NAMES
    openjpeg.h
  HINTS
    ${_openjpeg_SEARCH_DIRS}
  PATH_SUFFIXES
    include
    include/openjpeg-1.5
)

FIND_LIBRARY(OPENJPEG_LIBRARY
  NAMES
    openjpeg
  HINTS
    ${_openjpeg_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set OPENJPEG_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenJPEG DEFAULT_MSG
    OPENJPEG_LIBRARY OPENJPEG_INCLUDE_DIR)

IF(OPENJPEG_FOUND)
  SET(OPENJPEG_LIBRARIES ${OPENJPEG_LIBRARY})
  SET(OPENJPEG_INCLUDE_DIRS ${OPENJPEG_INCLUDE_DIR})
ENDIF(OPENJPEG_FOUND)

MARK_AS_ADVANCED(
  OPENJPEG_INCLUDE_DIR
  OPENJPEG_LIBRARY
)
