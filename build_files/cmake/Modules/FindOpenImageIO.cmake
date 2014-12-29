# - Find OpenImageIO library
# Find the native OpenImageIO includes and library
# This module defines
#  OPENIMAGEIO_INCLUDE_DIRS, where to find openimageio.h, Set when
#                            OPENIMAGEIO_INCLUDE_DIR is found.
#  OPENIMAGEIO_LIBRARIES, libraries to link against to use OpenImageIO.
#  OPENIMAGEIO_ROOT_DIR, The base directory to search for OpenImageIO.
#                        This can also be an environment variable.
#  OPENIMAGEIO_FOUND, If false, do not try to use OpenImageIO.
#  OPENIMAGEIO_PUGIXML_FOUND, Indicates whether OIIO has biltin PuguXML parser.
#
# also defined, but not for general use are
#  OPENIMAGEIO_LIBRARY, where to find the OpenImageIO library.

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

# If OPENIMAGEIO_ROOT_DIR was defined in the environment, use it.
IF(NOT OPENIMAGEIO_ROOT_DIR AND NOT $ENV{OPENIMAGEIO_ROOT_DIR} STREQUAL "")
  SET(OPENIMAGEIO_ROOT_DIR $ENV{OPENIMAGEIO_ROOT_DIR})
ENDIF()

SET(_openimageio_SEARCH_DIRS
  ${OPENIMAGEIO_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt/lib/oiio
)

FIND_PATH(OPENIMAGEIO_INCLUDE_DIR
  NAMES
    OpenImageIO/imageio.h
  HINTS
    ${_openimageio_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(OPENIMAGEIO_LIBRARY
  NAMES
    OpenImageIO
  HINTS
    ${_openimageio_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set OPENIMAGEIO_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenImageIO DEFAULT_MSG
    OPENIMAGEIO_LIBRARY OPENIMAGEIO_INCLUDE_DIR)

IF(OPENIMAGEIO_FOUND)
  SET(OPENIMAGEIO_LIBRARIES ${OPENIMAGEIO_LIBRARY})
  SET(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO_INCLUDE_DIR})
  IF(EXISTS ${OPENIMAGEIO_INCLUDE_DIR}/OpenImageIO/pugixml.hpp)
    SET(OPENIMAGEIO_PUGIXML_FOUND TRUE)
  ENDIF()
ELSE()
  SET(OPENIMAGEIO_PUGIXML_FOUND FALSE)
ENDIF()

MARK_AS_ADVANCED(
  OPENIMAGEIO_INCLUDE_DIR
  OPENIMAGEIO_LIBRARY
)

UNSET(_openimageio_SEARCH_DIRS)
