# - Find Universal Scene Description (USD) library
# Find the native USD includes and libraries
# This module defines
#  USD_INCLUDE_DIRS, where to find USD headers, Set when
#                        USD_INCLUDE_DIR is found.
#  USD_LIBRARIES, libraries to link against to use USD.
#  USD_ROOT_DIR, The base directory to search for USD.
#                    This can also be an environment variable.
#  USD_FOUND, If false, do not try to use USD.
#

#=============================================================================
# Copyright 2019 Blender Foundation.
#
# Distributed under the OSI-approved BSD 3-Clause License,
# see accompanying file BSD-3-Clause-license.txt for details.
#=============================================================================

# If USD_ROOT_DIR was defined in the environment, use it.
IF(NOT USD_ROOT_DIR AND NOT $ENV{USD_ROOT_DIR} STREQUAL "")
  SET(USD_ROOT_DIR $ENV{USD_ROOT_DIR})
ENDIF()

SET(_usd_SEARCH_DIRS
  ${USD_ROOT_DIR}
  /opt/lib/usd
)

FIND_PATH(USD_INCLUDE_DIR
  NAMES
    pxr/usd/usd/api.h
  HINTS
    ${_usd_SEARCH_DIRS}
  PATH_SUFFIXES
    include
  DOC "Universal Scene Description (USD) header files"
)

FIND_LIBRARY(USD_LIBRARY
  NAMES
    usd_m usd_ms
  NAMES_PER_DIR
  HINTS
    ${_usd_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib lib/static
  DOC "Universal Scene Description (USD) monolithic library"
)

IF(${USD_LIBRARY_NOTFOUND})
  set(USD_FOUND FALSE)
ELSE()
  # handle the QUIETLY and REQUIRED arguments and set USD_FOUND to TRUE if
  # all listed variables are TRUE
  INCLUDE(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(USD DEFAULT_MSG USD_LIBRARY USD_INCLUDE_DIR)

  IF(USD_FOUND)
    get_filename_component(USD_LIBRARY_DIR ${USD_LIBRARY} DIRECTORY)
    SET(USD_INCLUDE_DIRS ${USD_INCLUDE_DIR})
    set(USD_LIBRARIES ${USD_LIBRARY})
  ENDIF(USD_FOUND)
ENDIF()

MARK_AS_ADVANCED(
  USD_INCLUDE_DIR
  USD_LIBRARY_DIR
)

UNSET(_usd_SEARCH_DIRS)
