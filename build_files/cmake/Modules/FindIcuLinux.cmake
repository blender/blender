# - Find static icu libraries
# Find the native static icu libraries (needed for static boost_locale :/ ).
# This module defines
#  ICU_LIBRARIES, libraries to link against to use icu.
#  ICU_ROOT_DIR, The base directory to search for icu.
#                    This can also be an environment variable.
#  ICU_FOUND, If false, do not try to use icu.
#
# also defined, but not for general use are
#  ICU_LIBRARY_xxx, where to find the icu libraries.

#=============================================================================
# Copyright 2012 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# If ICU_ROOT_DIR was defined in the environment, use it.
IF(NOT ICU_ROOT_DIR AND NOT $ENV{ICU_ROOT_DIR} STREQUAL "")
  SET(ICU_ROOT_DIR $ENV{ICU_ROOT_DIR})
ENDIF()

if(Boost_USE_STATIC_LIBS)
  set(_icu_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()

SET(_icu_SEARCH_DIRS
  ${ICU_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
)

# We don't need includes, only libs to link against...
#FIND_PATH(ICU_INCLUDE_DIR
#  NAMES
#    utf.h
#  HINTS
#    ${_icu_SEARCH_DIRS}
#  PATH_SUFFIXES
#    include/unicode
#)

FIND_LIBRARY(ICU_LIBRARY_DATA
  NAMES
    icudata
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

FIND_LIBRARY(ICU_LIBRARY_I18N
  NAMES
    icui18n
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

FIND_LIBRARY(ICU_LIBRARY_IO
  NAMES
    icuio
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

FIND_LIBRARY(ICU_LIBRARY_LE
  NAMES
    icule
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

FIND_LIBRARY(ICU_LIBRARY_LX
  NAMES
    iculx
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

FIND_LIBRARY(ICU_LIBRARY_TU
  NAMES
    icutu
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

FIND_LIBRARY(ICU_LIBRARY_UC
  NAMES
    icuuc
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# Restore the original find library ordering
if(Boost_USE_STATIC_LIBS)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${_icu_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

# handle the QUIETLY and REQUIRED arguments and set ICU_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Icu DEFAULT_MSG
    ICU_LIBRARY_DATA
    ICU_LIBRARY_I18N
    ICU_LIBRARY_IO
    ICU_LIBRARY_LE
    ICU_LIBRARY_LX
    ICU_LIBRARY_TU
    ICU_LIBRARY_UC
)

IF(ICU_FOUND)
  SET(ICU_LIBRARIES ${ICU_LIBRARY_DATA} ${ICU_LIBRARY_I18N} ${ICU_LIBRARY_IO} ${ICU_LIBRARY_LE} ${ICU_LIBRARY_LX} ${ICU_LIBRARY_TU} ${ICU_LIBRARY_UC})
  SET(ICU_INCLUDE_DIRS ${ICU_INCLUDE_DIR})
ENDIF(ICU_FOUND)

MARK_AS_ADVANCED(
  ICU_INCLUDE_DIR
  ICU_LIBRARY_DATA
  ICU_LIBRARY_I18N
  ICU_LIBRARY_IO
  ICU_LIBRARY_LE
  ICU_LIBRARY_LX
  ICU_LIBRARY_TU
  ICU_LIBRARY_UC
)
