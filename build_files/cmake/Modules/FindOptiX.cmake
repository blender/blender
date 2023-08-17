# SPDX-FileCopyrightText: 2019 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find OptiX library
# Find the native OptiX includes and library
# This module defines
#  OPTIX_INCLUDE_DIRS, where to find optix.h, Set when
#                         OPTIX_INCLUDE_DIR is found.
#  OPTIX_ROOT_DIR, The base directory to search for OptiX.
#                     This can also be an environment variable.
#  OPTIX_FOUND, If false, do not try to use OptiX.

# If `OPTIX_ROOT_DIR` was defined in the environment, use it.
if(DEFINED OPTIX_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{OPTIX_ROOT_DIR})
  set(OPTIX_ROOT_DIR $ENV{OPTIX_ROOT_DIR})
else()
  set(OPTIX_ROOT_DIR "")
endif()

set(_optix_SEARCH_DIRS
  ${OPTIX_ROOT_DIR}
)

# TODO: Which environment uses this?
if(DEFINED ENV{PROGRAMDATA})
  list(APPEND _optix_SEARCH_DIRS "$ENV{PROGRAMDATA}/NVIDIA Corporation/OptiX SDK 7.3.0")
endif()

find_path(OPTIX_INCLUDE_DIR
  NAMES
    optix.h
  HINTS
    ${_optix_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

if(EXISTS "${OPTIX_INCLUDE_DIR}/optix.h")
  file(STRINGS "${OPTIX_INCLUDE_DIR}/optix.h" _optix_version REGEX "^#define OPTIX_VERSION[ \t].*$")
  string(REGEX MATCHALL "[0-9]+" _optix_version ${_optix_version})

  math(EXPR _optix_version_major "${_optix_version} / 10000")
  math(EXPR _optix_version_minor "(${_optix_version} % 10000) / 100")
  math(EXPR _optix_version_patch "${_optix_version} % 100")

  set(OPTIX_VERSION "${_optix_version_major}.${_optix_version_minor}.${_optix_version_patch}")
endif()

# handle the QUIETLY and REQUIRED arguments and set OPTIX_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OptiX
    REQUIRED_VARS OPTIX_INCLUDE_DIR
    VERSION_VAR OPTIX_VERSION)

if(OPTIX_FOUND)
  set(OPTIX_INCLUDE_DIRS ${OPTIX_INCLUDE_DIR})
endif()

mark_as_advanced(
  OPTIX_INCLUDE_DIR
  OPTIX_VERSION
)

unset(_optix_SEARCH_DIRS)
