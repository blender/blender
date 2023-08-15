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
IF(DEFINED OPENJPEG_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{OPENJPEG_ROOT_DIR})
  SET(OPENJPEG_ROOT_DIR $ENV{OPENJPEG_ROOT_DIR})
ELSE()
  SET(OPENJPEG_ROOT_DIR "")
ENDIF()

SET(_openjpeg_SEARCH_DIRS
  ${OPENJPEG_ROOT_DIR}
)

FIND_PATH(OPENJPEG_INCLUDE_DIR
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

FIND_LIBRARY(OPENJPEG_LIBRARY
  NAMES
    openjp2
  HINTS
    ${_openjpeg_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set OPENJPEG_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenJPEG DEFAULT_MSG
    OPENJPEG_LIBRARY OPENJPEG_INCLUDE_DIR)

IF(OPENJPEG_FOUND)
  SET(OPENJPEG_LIBRARIES ${OPENJPEG_LIBRARY})
  SET(OPENJPEG_INCLUDE_DIRS ${OPENJPEG_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  OPENJPEG_INCLUDE_DIR
  OPENJPEG_LIBRARY
)

UNSET(_openjpeg_SEARCH_DIRS)
