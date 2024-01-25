# SPDX-FileCopyrightText: 2011 Blender Authors
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
#  OPENIMAGEIO_TOOL, full path to oiiotool application if found.
#
# also defined, but not for general use are
#  OPENIMAGEIO_LIBRARY, where to find the OpenImageIO library.

# If `OPENIMAGEIO_ROOT_DIR` was defined in the environment, use it.
if(DEFINED OPENIMAGEIO_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{OPENIMAGEIO_ROOT_DIR})
  set(OPENIMAGEIO_ROOT_DIR $ENV{OPENIMAGEIO_ROOT_DIR})
else()
  set(OPENIMAGEIO_ROOT_DIR "")
endif()

set(_openimageio_SEARCH_DIRS
  ${OPENIMAGEIO_ROOT_DIR}
  /opt/lib/oiio
)

find_path(OPENIMAGEIO_INCLUDE_DIR
  NAMES
    OpenImageIO/imageio.h
  HINTS
    ${_openimageio_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(OPENIMAGEIO_LIBRARY
  NAMES
    OpenImageIO
  HINTS
    ${_openimageio_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

set(_openimageio_LIBRARIES ${OPENIMAGEIO_LIBRARY})

find_file(OPENIMAGEIO_TOOL
  NAMES
    oiiotool
  HINTS
    ${_openimageio_SEARCH_DIRS}
  PATH_SUFFIXES
    bin
)

# Additionally find util library if needed. In old versions this library was
# included in libOpenImageIO and linking to both would duplicate symbols. In
# new versions we need to link to both.
find_file(_openimageio_export
  NAMES
    export.h
  PATHS
    ${OPENIMAGEIO_INCLUDE_DIR}/OpenImageIO
  NO_DEFAULT_PATH
)

# Use existence of OIIO_UTIL_API to check if it's a separate lib.
file(STRINGS "${_openimageio_export}" _openimageio_util_define
     REGEX "^[ \t]*#[ \t]*define[ \t]+OIIO_UTIL_API.*$")

if(_openimageio_util_define)
  find_library(OPENIMAGEIO_UTIL_LIBRARY
    NAMES
      OpenImageIO_Util
    HINTS
      ${_openimageio_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )

  list(APPEND _openimageio_LIBRARIES ${OPENIMAGEIO_UTIL_LIBRARY})
endif()

# In cmake version 3.21 and up, we can instead use the NO_CACHE option for
# FIND_FILE so we don't need to clear it from the cache here.
unset(_openimageio_export CACHE)
unset(_openimageio_util_define)

# handle the QUIETLY and REQUIRED arguments and set OPENIMAGEIO_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenImageIO DEFAULT_MSG
    _openimageio_LIBRARIES OPENIMAGEIO_INCLUDE_DIR)

if(OPENIMAGEIO_FOUND)
  set(OPENIMAGEIO_LIBRARIES ${_openimageio_LIBRARIES})
  set(OPENIMAGEIO_INCLUDE_DIRS ${OPENIMAGEIO_INCLUDE_DIR})
  if(EXISTS ${OPENIMAGEIO_INCLUDE_DIR}/OpenImageIO/pugixml.hpp)
    set(OPENIMAGEIO_PUGIXML_FOUND TRUE)
  else()
    set(OPENIMAGEIO_PUGIXML_FOUND FALSE)
  endif()
else()
  set(OPENIMAGEIO_PUGIXML_FOUND FALSE)
endif()

mark_as_advanced(
  OPENIMAGEIO_INCLUDE_DIR
  OPENIMAGEIO_LIBRARY
  OPENIMAGEIO_UTIL_LIBRARY
  OPENIMAGEIO_TOOL
)

unset(_openimageio_SEARCH_DIRS)
unset(_openimageio_LIBRARIES)
