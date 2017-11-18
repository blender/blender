# - Find HDF5 library
# Find the native HDF5 includes and libraries
# This module defines
#  HDF5_INCLUDE_DIRS, where to find hdf5.h, Set when HDF5_INCLUDE_DIR is found.
#  HDF5_LIBRARIES, libraries to link against to use HDF5.
#  HDF5_ROOT_DIR, The base directory to search for HDF5.
#                 This can also be an environment variable.
#  HDF5_FOUND, If false, do not try to use HDF5.
#

#=============================================================================
# Copyright 2016 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# If HDF5_ROOT_DIR was defined in the environment, use it.
IF(NOT HDF5_ROOT_DIR AND NOT $ENV{HDF5_ROOT_DIR} STREQUAL "")
  SET(HDF5_ROOT_DIR $ENV{HDF5_ROOT_DIR})
ENDIF()

SET(_hdf5_SEARCH_DIRS
  ${HDF5_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/lib/hdf5
)

FIND_LIBRARY(HDF5_LIBRARY
  NAMES
    hdf5
  HINTS
    ${_hdf5_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
)

FIND_PATH(HDF5_INCLUDE_DIR
  NAMES
    hdf5.h
  HINTS
    ${_hdf5_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

# handle the QUIETLY and REQUIRED arguments and set HDF5_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(HDF5 DEFAULT_MSG HDF5_LIBRARY HDF5_INCLUDE_DIR)

IF(HDF5_FOUND)
  SET(HDF5_LIBRARIES ${HDF5_LIBRARY})
  SET(HDF5_INCLUDE_DIRS ${HDF5_INCLUDE_DIR})
ENDIF(HDF5_FOUND)

MARK_AS_ADVANCED(
  HDF5_INCLUDE_DIR
  HDF5_LIBRARY
)

UNSET(_hdf5_SEARCH_DIRS)
