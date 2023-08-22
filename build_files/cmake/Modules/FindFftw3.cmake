# SPDX-FileCopyrightText: 2011 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Fftw3 library
# Find the native Fftw3 includes and library
# This module defines
#  FFTW3_INCLUDE_DIRS, where to find fftw3.h, Set when
#                        FFTW3_INCLUDE_DIR is found.
#  FFTW3_LIBRARIES, libraries to link against to use Fftw3.
#  FFTW3_ROOT_DIR, The base directory to search for Fftw3.
#                    This can also be an environment variable.
#  FFTW3_FOUND, If false, do not try to use Fftw3.
#
# also defined, but not for general use are
#  FFTW3_LIBRARY, where to find the Fftw3 library.

# If `FFTW3_ROOT_DIR` was defined in the environment, use it.
if(DEFINED FFTW3_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{FFTW3_ROOT_DIR})
  set(FFTW3_ROOT_DIR $ENV{FFTW3_ROOT_DIR})
else()
  set(FFTW3_ROOT_DIR "")
endif()

set(_fftw3_SEARCH_DIRS
  ${FFTW3_ROOT_DIR}
)

find_path(FFTW3_INCLUDE_DIR
  NAMES
    fftw3.h
  HINTS
    ${_fftw3_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(FFTW3_LIBRARY
  NAMES
    fftw3
  HINTS
    ${_fftw3_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set FFTW3_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Fftw3 DEFAULT_MSG
    FFTW3_LIBRARY FFTW3_INCLUDE_DIR)

if(FFTW3_FOUND)
  set(FFTW3_LIBRARIES ${FFTW3_LIBRARY})
  set(FFTW3_INCLUDE_DIRS ${FFTW3_INCLUDE_DIR})
endif()

mark_as_advanced(
  FFTW3_INCLUDE_DIR
  FFTW3_LIBRARY
)
