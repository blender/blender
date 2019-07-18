# - Find SndFile library
# Find the native SndFile includes and library
# This module defines
#  LIBSNDFILE_INCLUDE_DIRS, where to find sndfile.h, Set when
#                        LIBSNDFILE_INCLUDE_DIR is found.
#  LIBSNDFILE_LIBRARIES, libraries to link against to use SndFile.
#  LIBSNDFILE_ROOT_DIR, The base directory to search for SndFile.
#                    This can also be an environment variable.
#  SNDFILE_FOUND, If false, do not try to use SndFile.
#
# also defined, but not for general use are
#  LIBSNDFILE_LIBRARY, where to find the SndFile library.

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

# If LIBSNDFILE_ROOT_DIR was defined in the environment, use it.
IF(NOT LIBSNDFILE_ROOT_DIR AND NOT $ENV{LIBSNDFILE_ROOT_DIR} STREQUAL "")
  SET(LIBSNDFILE_ROOT_DIR $ENV{LIBSNDFILE_ROOT_DIR})
ENDIF()

SET(_sndfile_SEARCH_DIRS
  ${LIBSNDFILE_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
)

FIND_PATH(LIBSNDFILE_INCLUDE_DIR sndfile.h
  HINTS
    ${_sndfile_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(LIBSNDFILE_LIBRARY
  NAMES
    sndfile
  HINTS
    ${_sndfile_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set SNDFILE_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SndFile DEFAULT_MSG
  LIBSNDFILE_LIBRARY LIBSNDFILE_INCLUDE_DIR)

IF(SNDFILE_FOUND)
  SET(LIBSNDFILE_LIBRARIES ${LIBSNDFILE_LIBRARY})
  SET(LIBSNDFILE_INCLUDE_DIRS ${LIBSNDFILE_INCLUDE_DIR})
ENDIF(SNDFILE_FOUND)

MARK_AS_ADVANCED(
  LIBSNDFILE_INCLUDE_DIR
  LIBSNDFILE_LIBRARY
)
