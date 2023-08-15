# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find ShaderC libraries
# Find the ShaderC includes and libraries
# This module defines
#  SHADERC_INCLUDE_DIRS, where to find MoltenVK headers, Set when
#                        SHADERC_INCLUDE_DIR is found.
#  SHADERC_LIBRARIES, libraries to link against to use ShaderC.
#  SHADERC_ROOT_DIR, The base directory to search for ShaderC.
#                    This can also be an environment variable.
#  SHADERC_FOUND, If false, do not try to use ShaderC.
#

# If `SHADERC_ROOT_DIR` was defined in the environment, use it.
IF(DEFINED SHADERC_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{SHADERC_ROOT_DIR})
  SET(SHADERC_ROOT_DIR $ENV{SHADERC_ROOT_DIR})
ELSE()
  SET(SHADERC_ROOT_DIR "")
ENDIF()

SET(_shaderc_SEARCH_DIRS
  ${SHADERC_ROOT_DIR}
)

# FIXME: These finder modules typically don't use LIBDIR,
# this should be set by `./build_files/cmake/platform/` instead.
IF(DEFINED LIBDIR)
  SET(_shaderc_SEARCH_DIRS ${_shaderc_SEARCH_DIRS} ${LIBDIR}/shaderc)
ENDIF()

FIND_PATH(SHADERC_INCLUDE_DIR
  NAMES
    shaderc/shaderc.h
  HINTS
    ${_shaderc_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(SHADERC_LIBRARY
  NAMES
    shaderc_combined
  HINTS
    ${_shaderc_SEARCH_DIRS}
  PATH_SUFFIXES
    lib
)

# handle the QUIETLY and REQUIRED arguments and set SHADERC_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ShaderC DEFAULT_MSG SHADERC_LIBRARY SHADERC_INCLUDE_DIR)

IF(SHADERC_FOUND)
  SET(SHADERC_LIBRARIES ${SHADERC_LIBRARY})
  SET(SHADERC_INCLUDE_DIRS ${SHADERC_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  SHADERC_INCLUDE_DIR
  SHADERC_LIBRARY
)

UNSET(_shaderc_SEARCH_DIRS)
