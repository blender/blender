# - Find Fftw3 library
# Find the native Fftw3 includes and library
# This module defines
#  FFTW3_INCLUDE_DIRS, where to find fftw3.h, Set when
#                        FFTW3_INCLUDE_DIR is found.
#  FFTW3_LIBRARIES, libraries to link against to use Fftw3.
#  FFTW3_ROOT_DIR, The base directory to search for Fftw3.
#                    This can also be an environment variable.
#  FFTW3_FOUND, If false, do not try to use Fftw3.
#
# also defined, but not for general use are
#  FFTW3_LIBRARY, where to find the Fftw3 library.

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

# If FFTW3_ROOT_DIR was defined in the environment, use it.
IF(NOT FFTW3_ROOT_DIR AND NOT $ENV{FFTW3_ROOT_DIR} STREQUAL "")
  SET(FFTW3_ROOT_DIR $ENV{FFTW3_ROOT_DIR})
ENDIF()

SET(_fftw3_SEARCH_DIRS
  ${FFTW3_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
)

FIND_PATH(FFTW3_INCLUDE_DIR
  NAMES
    fftw3.h
  HINTS
    ${_fftw3_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(FFTW3_LIBRARY
  NAMES
    fftw3
  HINTS
    ${_fftw3_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set FFTW3_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Fftw3 DEFAULT_MSG
    FFTW3_LIBRARY FFTW3_INCLUDE_DIR)

IF(FFTW3_FOUND)
  SET(FFTW3_LIBRARIES ${FFTW3_LIBRARY})
  SET(FFTW3_INCLUDE_DIRS ${FFTW3_INCLUDE_DIR})
ENDIF(FFTW3_FOUND)

MARK_AS_ADVANCED(
  FFTW3_INCLUDE_DIR
  FFTW3_LIBRARY
)
