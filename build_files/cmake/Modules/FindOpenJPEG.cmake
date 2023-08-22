# SPDX-FileCopyrightText: 2011 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find OpenJPEG library
# Find the native OpenJPEG includes and library
# This module defines
#  OPENJPEG_INCLUDE_DIRS, where to find openjpeg.h, Set when
#                        OPENJPEG_INCLUDE_DIR is found.
#  OPENJPEG_LIBRARIES, libraries to link against to use OpenJPEG.
#  OPENJPEG_ROOT_DIR, The base directory to search for OpenJPEG.
#                    This can also be an environment variable.
#  OPENJPEG_FOUND, If false, do not try to use OpenJPEG.
#
# also defined, but not for general use are
#  OPENJPEG_LIBRARY, where to find the OpenJPEG library.

# If `OPENJPEG_ROOT_DIR` was defined in the environment, use it.
if(DEFINED OPENJPEG_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{OPENJPEG_ROOT_DIR})
  set(OPENJPEG_ROOT_DIR $ENV{OPENJPEG_ROOT_DIR})
else()
  set(OPENJPEG_ROOT_DIR "")
endif()

set(_openjpeg_SEARCH_DIRS
  ${OPENJPEG_ROOT_DIR}
)

find_path(OPENJPEG_INCLUDE_DIR
  NAMES
    openjpeg.h
  HINTS
    ${_openjpeg_SEARCH_DIRS}
  PATH_SUFFIXES
    include
    # Support future versions
    openjpeg-2.9
    openjpeg-2.8
    openjpeg-2.7
    openjpeg-2.6
    openjpeg-2.5
    openjpeg-2.4
    openjpeg-2.3
    openjpeg-2.2
    openjpeg-2.1
    openjpeg-2.0
)

find_library(OPENJPEG_LIBRARY
  NAMES
    openjp2
  HINTS
    ${_openjpeg_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set OPENJPEG_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenJPEG DEFAULT_MSG
    OPENJPEG_LIBRARY OPENJPEG_INCLUDE_DIR)

if(OPENJPEG_FOUND)
  set(OPENJPEG_LIBRARIES ${OPENJPEG_LIBRARY})
  set(OPENJPEG_INCLUDE_DIRS ${OPENJPEG_INCLUDE_DIR})
endif()

mark_as_advanced(
  OPENJPEG_INCLUDE_DIR
  OPENJPEG_LIBRARY
)

unset(_openjpeg_SEARCH_DIRS)
