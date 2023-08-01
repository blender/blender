# SPDX-FileCopyrightText: 2011 Blender Foundation
#
# SPDX-License-Identifier: BSD-3-Clause

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
#  OPENIMAGEIO_IDIFF, full path to idiff application if found.
#
# also defined, but not for general use are
#  OPENIMAGEIO_LIBRARY, where to find the OpenImageIO library.

# If `OPENIMAGEIO_ROOT_DIR` was defined in the environment, use it.
IF(DEFINED OPENIMAGEIO_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{OPENIMAGEIO_ROOT_DIR})
  SET(OPENIMAGEIO_ROOT_DIR $ENV{OPENIMAGEIO_ROOT_DIR})
ELSE()
  SET(OPENIMAGEIO_ROOT_DIR "")
ENDIF()

SET(_openimageio_SEARCH_DIRS
  ${OPENIMAGEIO_ROOT_DIR}
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

set(_openimageio_LIBRARIES ${OPENIMAGEIO_LIBRARY})

FIND_FILE(OPENIMAGEIO_IDIFF
  NAMES
    idiff
  HINTS
    ${_openimageio_SEARCH_DIRS}
  PATH_SUFFIXES
    bin
)

# Additionally find util library if needed. In old versions this library was
# included in libOpenImageIO and linking to both would duplicate symbols. In
# new versions we need to link to both.
FIND_FILE(_openimageio_export
  NAMES
    export.h
  PATHS
    ${OPENIMAGEIO_INCLUDE_DIR}/OpenImageIO
  NO_DEFAULT_PATH
)

# Use existence of OIIO_UTIL_API to check if it's a separate lib.
FILE(STRINGS "${_openimageio_export}" _openimageio_util_define
     REGEX "^[ \t]*#[ \t]*define[ \t]+OIIO_UTIL_API.*$")

IF(_openimageio_util_define)
  FIND_LIBRARY(OPENIMAGEIO_UTIL_LIBRARY
    NAMES
      OpenImageIO_Util
    HINTS
      ${_openimageio_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )

  LIST(APPEND _openimageio_LIBRARIES ${OPENIMAGEIO_UTIL_LIBRARY})
ENDIF()

# In cmake version 3.21 and up, we can instead use the NO_CACHE option for
# FIND_FILE so we don't need to clear it from the cache here.
UNSET(_openimageio_export CACHE)
UNSET(_openimageio_util_define)

# handle the QUIETLY and REQUIRED arguments and set OPENIMAGEIO_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenImageIO DEFAULT_MSG
    _openimageio_LIBRARIES OPENIMAGEIO_INCLUDE_DIR)

IF(OPENIMAGEIO_FOUND)
  SET(OPENIMAGEIO_LIBRARIES ${_openimageio_LIBRARIES})
  SET(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO_INCLUDE_DIR})
  IF(EXISTS ${OPENIMAGEIO_INCLUDE_DIR}/OpenImageIO/pugixml.hpp)
    SET(OPENIMAGEIO_PUGIXML_FOUND TRUE)
  ELSE()
    SET(OPENIMAGEIO_PUGIXML_FOUND FALSE)
  ENDIF()
ELSE()
  SET(OPENIMAGEIO_PUGIXML_FOUND FALSE)
ENDIF()

MARK_AS_ADVANCED(
  OPENIMAGEIO_INCLUDE_DIR
  OPENIMAGEIO_LIBRARY
  OPENIMAGEIO_UTIL_LIBRARY
  OPENIMAGEIO_IDIFF
)

UNSET(_openimageio_SEARCH_DIRS)
UNSET(_openimageio_LIBRARIES)
