# - Find OptiX library
# Find the native OptiX includes and library
# This module defines
#  OPTIX_INCLUDE_DIRS, where to find optix.h, Set when
#                         OPTIX_INCLUDE_DIR is found.
#  OPTIX_ROOT_DIR, The base directory to search for OptiX.
#                     This can also be an environment variable.
#  OPTIX_FOUND, If false, do not try to use OptiX.

#=============================================================================
# Copyright 2019 Blender Foundation.
#
# Distributed under the OSI-approved BSD 3-Clause License,
# see accompanying file BSD-3-Clause-license.txt for details.
#=============================================================================

# If OPTIX_ROOT_DIR was defined in the environment, use it.
IF(NOT OPTIX_ROOT_DIR AND NOT $ENV{OPTIX_ROOT_DIR} STREQUAL "")
  SET(OPTIX_ROOT_DIR $ENV{OPTIX_ROOT_DIR})
ENDIF()

SET(_optix_SEARCH_DIRS
  ${OPTIX_ROOT_DIR}
  "$ENV{PROGRAMDATA}/NVIDIA Corporation/OptiX SDK 7.0.0"
)

FIND_PATH(OPTIX_INCLUDE_DIR
  NAMES
    optix.h
  HINTS
    ${_optix_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

# handle the QUIETLY and REQUIRED arguments and set OPTIX_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OptiX DEFAULT_MSG
    OPTIX_INCLUDE_DIR)

IF(OPTIX_FOUND)
  SET(OPTIX_INCLUDE_DIRS ${OPTIX_INCLUDE_DIR})
ENDIF(OPTIX_FOUND)

MARK_AS_ADVANCED(
  OPTIX_INCLUDE_DIR
)

UNSET(_optix_SEARCH_DIRS)
