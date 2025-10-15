# SPDX-FileCopyrightText: 2020 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find FFmpeg library and includes.
# Set FFMPEG_FIND_COMPONENTS to the canonical names of the libraries
# before using the module.
# This module defines
#  FFMPEG_INCLUDE_DIRS, where to find libavcodec/ac3_parser.h.
#  FFMPEG_LIBRARIES, libraries to link against to use FFmpeg.
#  FFMPEG_ROOT_DIR, The base directory to search for FFmpeg.
#                        This can also be an environment variable.
#  FFMPEG_FOUND, If false, do not try to use FFmpeg.
#  FFMPEG_<COMPONENT>_LIBRARY, the given individual component libraries.

# If `FFMPEG_ROOT_DIR` was defined in the environment, use it.
if(DEFINED FFMPEG_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{FFMPEG_ROOT_DIR})
  set(FFMPEG_ROOT_DIR $ENV{FFMPEG_ROOT_DIR})
else()
  set(FFMPEG_ROOT_DIR "")
endif()

set(_ffmpeg_SEARCH_DIRS
  ${FFMPEG_ROOT_DIR}
  /opt/lib/ffmpeg
)

if(NOT FFMPEG_FIND_COMPONENTS)
  set(FFMPEG_FIND_COMPONENTS
    # List taken from http://ffmpeg.org/download.html#build-mac
    avcodec
    avdevice
    avfilter
    avformat
    avutil
    swscale
    swresample
  )
endif()

find_path(_ffmpeg_INCLUDE_DIR
  NAMES
    libavcodec/ac3_parser.h
  HINTS
    ${_ffmpeg_SEARCH_DIRS}
  PATH_SUFFIXES
    include
    # Used by `ffmpeg-devel` on Fedora (see #147952).
    include/ffmpeg
)

set(_ffmpeg_LIBRARIES)
foreach(_component ${FFMPEG_FIND_COMPONENTS})
  string(TOUPPER ${_component} _upper_COMPONENT)
  find_library(FFMPEG_${_upper_COMPONENT}_LIBRARY
    NAMES
      ${_component}
    HINTS
      ${_ffmpeg_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
  )
  if(NOT FFMPEG_${_upper_COMPONENT}_LIBRARY)
    message(WARNING "Could NOT find FFmpeg ${_upper_COMPONENT}.")
  endif()
  list(APPEND _ffmpeg_LIBRARIES ${FFMPEG_${_upper_COMPONENT}_LIBRARY})
  mark_as_advanced(FFMPEG_${_upper_COMPONENT}_LIBRARY)
endforeach()
unset(_component)
unset(_upper_COMPONENT)

# handle the QUIETLY and REQUIRED arguments and set FFMPEG_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg DEFAULT_MSG
  _ffmpeg_LIBRARIES _ffmpeg_INCLUDE_DIR)

if(FFMPEG_FOUND)
  set(FFMPEG_LIBRARIES ${_ffmpeg_LIBRARIES})
  set(FFMPEG_INCLUDE_DIRS ${_ffmpeg_INCLUDE_DIR})
endif()

mark_as_advanced(
  FFMPEG_INCLUDE_DIR
)

unset(_ffmpeg_SEARCH_DIRS)
unset(_ffmpeg_LIBRARIES)
# In cmake version 3.21 and up, we can instead use the NO_CACHE option for
# find_path so we don't need to clear it from the cache here.
unset(_ffmpeg_INCLUDE_DIR CACHE)
