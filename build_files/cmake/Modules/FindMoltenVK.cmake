# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Blender Foundation.

# - Find MoltenVK libraries
# Find the MoltenVK includes and libraries
# This module defines
#  MOLTENVK_INCLUDE_DIRS, where to find MoltenVK headers, Set when
#                        MOLTENVK_INCLUDE_DIR is found.
#  MOLTENVK_LIBRARIES, libraries to link against to use MoltenVK.
#  MOLTENVK_ROOT_DIR, The base directory to search for MoltenVK.
#                    This can also be an environment variable.
#  MOLTENVK_FOUND, If false, do not try to use MoltenVK.
#

# If MOLTENVK_ROOT_DIR was defined in the environment, use it.
IF(NOT MOLTENVK_ROOT_DIR AND NOT $ENV{MOLTENVK_ROOT_DIR} STREQUAL "")
  SET(MOLTENVK_ROOT_DIR $ENV{MOLTENVK_ROOT_DIR})
ENDIF()

SET(_moltenvk_SEARCH_DIRS
  ${MOLTENVK_ROOT_DIR}
)

# FIXME: These finder modules typically don't use LIBDIR,
# this should be set by `./build_files/cmake/platform/` instead.
IF(DEFINED LIBDIR)
  SET(_moltenvk_SEARCH_DIRS ${_moltenvk_SEARCH_DIRS} ${LIBDIR}/moltenvk)
ENDIF()

FIND_PATH(MOLTENVK_INCLUDE_DIR
  NAMES
    MoltenVK/vk_mvk_moltenvk.h
  HINTS
    ${_moltenvk_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(MOLTENVK_LIBRARY
  NAMES
    MoltenVK
  HINTS
    ${_moltenvk_SEARCH_DIRS}
  PATH_SUFFIXES
    dylib/macOS
)

# handle the QUIETLY and REQUIRED arguments and set MOLTENVK_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MoltenVK DEFAULT_MSG MOLTENVK_LIBRARY MOLTENVK_INCLUDE_DIR)

IF(MOLTENVK_FOUND)
  SET(MOLTENVK_LIBRARIES ${MOLTENVK_LIBRARY})
  SET(MOLTENVK_INCLUDE_DIRS ${MOLTENVK_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  MOLTENVK_INCLUDE_DIR
  MOLTENVK_LIBRARY
)

UNSET(_moltenvk_SEARCH_DIRS)
