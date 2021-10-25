# - Find SDL library
# Find the native SDL includes and library
# This module defines
#  SDL2_INCLUDE_DIRS, where to find SDL.h, Set when SDL2_INCLUDE_DIR is found.
#  SDL2_LIBRARIES, libraries to link against to use SDL.
#  SDL2_ROOT_DIR, The base directory to search for SDL.
#                This can also be an environment variable.
#  SDL2_FOUND, If false, do not try to use SDL.
#
# also defined, but not for general use are
#  SDL2_LIBRARY, where to find the SDL library.

#=============================================================================
# Copyright 2015 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# If SDL2_ROOT_DIR was defined in the environment, use it.
IF(NOT SDL2_ROOT_DIR AND NOT $ENV{SDL2_ROOT_DIR} STREQUAL "")
  SET(SDL2_ROOT_DIR $ENV{SDL2_ROOT_DIR})
ENDIF()

SET(_sdl2_SEARCH_DIRS
  ${SDL2_ROOT_DIR}
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
)

FIND_PATH(SDL2_INCLUDE_DIR
  NAMES
  SDL.h
  HINTS
    ${_sdl2_SEARCH_DIRS}
  PATH_SUFFIXES
    include/SDL2 include
)

FIND_LIBRARY(SDL2_LIBRARY
  NAMES
  SDL2
  HINTS
    ${_sdl2_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set SDL2_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SDL2 DEFAULT_MSG
    SDL2_LIBRARY SDL2_INCLUDE_DIR)

IF(SDL2_FOUND)
  SET(SDL2_LIBRARIES ${SDL2_LIBRARY})
  SET(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
ENDIF(SDL2_FOUND)

MARK_AS_ADVANCED(
  SDL2_INCLUDE_DIR
  SDL2_LIBRARY
)
