# SPDX-FileCopyrightText: 2015 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

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

# If `SDL2_ROOT_DIR` was defined in the environment, use it.
if(DEFINED SDL2_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{SDL2_ROOT_DIR})
  set(SDL2_ROOT_DIR $ENV{SDL2_ROOT_DIR})
else()
  set(SDL2_ROOT_DIR "")
endif()

set(_sdl2_SEARCH_DIRS
  ${SDL2_ROOT_DIR}
)

find_path(SDL2_INCLUDE_DIR
  NAMES
  SDL.h
  HINTS
    ${_sdl2_SEARCH_DIRS}
  PATH_SUFFIXES
    include/SDL2 include SDL2
)

find_library(SDL2_LIBRARY
  NAMES
  SDL2
  HINTS
    ${_sdl2_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set SDL2_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2 DEFAULT_MSG
    SDL2_LIBRARY SDL2_INCLUDE_DIR)

if(SDL2_FOUND)
  set(SDL2_LIBRARIES ${SDL2_LIBRARY})
  set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
endif()

mark_as_advanced(
  SDL2_INCLUDE_DIR
  SDL2_LIBRARY
)
