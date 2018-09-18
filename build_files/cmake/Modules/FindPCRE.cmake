# - Find PCRE library
# Find the native PCRE includes and library
# This module defines
#  PCRE_INCLUDE_DIRS, where to find pcre.h, Set when
#                     PCRE_INCLUDE_DIR is found.
#  PCRE_LIBRARIES, libraries to link against to use PCRE.
#  PCRE_ROOT_DIR, The base directory to search for PCRE.
#                 This can also be an environment variable.
#  PCRE_FOUND, If false, do not try to use PCRE.
#
# also defined, but not for general use are
#  PCRE_LIBRARY, where to find the PCRE library.

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

# If PCRE_ROOT_DIR was defined in the environment, use it.
IF(NOT PCRE_ROOT_DIR AND NOT $ENV{PCRE_ROOT_DIR} STREQUAL "")
  SET(PCRE_ROOT_DIR $ENV{PCRE_ROOT_DIR})
ENDIF()

SET(_pcre_SEARCH_DIRS
  ${PCRE_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
)

FIND_PATH(PCRE_INCLUDE_DIR pcre.h
  HINTS
    ${_pcre_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(PCRE_LIBRARY
  NAMES
    pcre
  HINTS
    ${_pcre_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set PCRE_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PCRE DEFAULT_MSG
    PCRE_LIBRARY PCRE_INCLUDE_DIR)

# With 'make deps' precompiled libs, opencollada ships with a copy of libpcre
# but not the headers, ${PCRE_LIBRARY} will be valid in this case
# but PCRE_FOUND will be FALSE. So we set this variable outside of
# the IF(PCRE_FOUND) below to allow blender to successfully link.
SET(PCRE_LIBRARIES ${PCRE_LIBRARY})

IF(PCRE_FOUND)
  SET(PCRE_INCLUDE_DIRS ${PCRE_INCLUDE_DIR})
ENDIF(PCRE_FOUND)

MARK_AS_ADVANCED(
  PCRE_INCLUDE_DIR
  PCRE_LIBRARY
)
