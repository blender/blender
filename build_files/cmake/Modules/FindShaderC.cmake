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
if(DEFINED SHADERC_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{SHADERC_ROOT_DIR})
  set(SHADERC_ROOT_DIR $ENV{SHADERC_ROOT_DIR})
else()
  set(SHADERC_ROOT_DIR "")
endif()

set(_shaderc_SEARCH_DIRS
  ${SHADERC_ROOT_DIR}
)

# FIXME: These finder modules typically don't use LIBDIR,
# this should be set by `./build_files/cmake/platform/` instead.
if(DEFINED LIBDIR)
  set(_shaderc_SEARCH_DIRS ${_shaderc_SEARCH_DIRS} ${LIBDIR}/shaderc)
endif()

find_path(SHADERC_INCLUDE_DIR
  NAMES
    shaderc/shaderc.h
  HINTS
    ${_shaderc_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(SHADERC_LIBRARY
  NAMES
    shaderc_combined
  HINTS
    ${_shaderc_SEARCH_DIRS}
  PATH_SUFFIXES
    lib
)

# handle the QUIETLY and REQUIRED arguments and set SHADERC_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ShaderC DEFAULT_MSG SHADERC_LIBRARY SHADERC_INCLUDE_DIR)

if(SHADERC_FOUND)
  set(SHADERC_LIBRARIES ${SHADERC_LIBRARY})
  set(SHADERC_INCLUDE_DIRS ${SHADERC_INCLUDE_DIR})
endif()

mark_as_advanced(
  SHADERC_INCLUDE_DIR
  SHADERC_LIBRARY
)

unset(_shaderc_SEARCH_DIRS)
