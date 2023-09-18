# SPDX-FileCopyrightText: 2011 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find SndFile library
# Find the native SndFile includes and library
# This module defines
#  LIBSNDFILE_INCLUDE_DIRS, where to find sndfile.h, Set when
#                        LIBSNDFILE_INCLUDE_DIR is found.
#  LIBSNDFILE_LIBRARIES, libraries to link against to use SndFile.
#  LIBSNDFILE_ROOT_DIR, The base directory to search for SndFile.
#                    This can also be an environment variable.
#  SNDFILE_FOUND, If false, do not try to use SndFile.
#
# also defined, but not for general use are
#  LIBSNDFILE_LIBRARY, where to find the SndFile library.

# If `LIBSNDFILE_ROOT_DIR` was defined in the environment, use it.
if(DEFINED LIBSNDFILE_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{LIBSNDFILE_ROOT_DIR})
  set(LIBSNDFILE_ROOT_DIR $ENV{LIBSNDFILE_ROOT_DIR})
else()
  set(LIBSNDFILE_ROOT_DIR "")
endif()

set(_sndfile_SEARCH_DIRS
  ${LIBSNDFILE_ROOT_DIR}
)

find_path(LIBSNDFILE_INCLUDE_DIR sndfile.h
  HINTS
    ${_sndfile_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(LIBSNDFILE_LIBRARY
  NAMES
    sndfile
  HINTS
    ${_sndfile_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set SNDFILE_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SndFile DEFAULT_MSG
  LIBSNDFILE_LIBRARY LIBSNDFILE_INCLUDE_DIR)

if(SNDFILE_FOUND)
  set(LIBSNDFILE_LIBRARIES ${LIBSNDFILE_LIBRARY})
  set(LIBSNDFILE_INCLUDE_DIRS ${LIBSNDFILE_INCLUDE_DIR})
endif()

mark_as_advanced(
  LIBSNDFILE_INCLUDE_DIR
  LIBSNDFILE_LIBRARY
)
