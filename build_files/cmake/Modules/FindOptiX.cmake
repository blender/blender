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
  "$ENV{PROGRAMDATA}/NVIDIA Corporation/OptiX SDK 7.3.0"
)

FIND_PATH(OPTIX_INCLUDE_DIR
  NAMES
    optix.h
  HINTS
    ${_optix_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

IF(EXISTS "${OPTIX_INCLUDE_DIR}/optix.h")
  FILE(STRINGS "${OPTIX_INCLUDE_DIR}/optix.h" _optix_version REGEX "^#define OPTIX_VERSION[ \t].*$")
  STRING(REGEX MATCHALL "[0-9]+" _optix_version ${_optix_version})

  MATH(EXPR _optix_version_major "${_optix_version} / 10000")
  MATH(EXPR _optix_version_minor "(${_optix_version} % 10000) / 100")
  MATH(EXPR _optix_version_patch "${_optix_version} % 100")

  SET(OPTIX_VERSION "${_optix_version_major}.${_optix_version_minor}.${_optix_version_patch}")
ENDIF()

# handle the QUIETLY and REQUIRED arguments and set OPTIX_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OptiX
    REQUIRED_VARS OPTIX_INCLUDE_DIR
    VERSION_VAR OPTIX_VERSION)

IF(OPTIX_FOUND)
  SET(OPTIX_INCLUDE_DIRS ${OPTIX_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  OPTIX_INCLUDE_DIR
  OPTIX_VERSION
)

UNSET(_optix_SEARCH_DIRS)
