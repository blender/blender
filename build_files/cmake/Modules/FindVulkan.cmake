# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Vulkan libraries
# Find the Vulkan includes and libraries
# This module defines
#  VULKAN_INCLUDE_DIRS, where to find Vulkan headers, Set when
#                       VULKAN_INCLUDE_DIR is found.
#  VULKAN_LIBRARIES, libraries to link against to use Vulkan.
#  VULKAN_ROOT_DIR, The base directory to search for Vulkan.
#                    This can also be an environment variable.
#  VULKAN_FOUND, If false, do not try to use Vulkan.
#

# If `VULKAN_ROOT_DIR` was defined in the environment, use it.
if(DEFINED VULKAN_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{VULKAN_ROOT_DIR})
  set(VULKAN_ROOT_DIR $ENV{VULKAN_ROOT_DIR})
else()
  set(VULKAN_ROOT_DIR "")
endif()

set(_vulkan_SEARCH_DIRS
  ${VULKAN_ROOT_DIR}
)

# FIXME: These finder modules typically don't use LIBDIR,
# this should be set by `./build_files/cmake/platform/` instead.
if(DEFINED LIBDIR)
  set(_vulkan_SEARCH_DIRS ${_vulkan_SEARCH_DIRS} ${LIBDIR}/vulkan)
endif()

find_path(VULKAN_INCLUDE_DIR
  NAMES
    vulkan/vulkan.h
  HINTS
    ${_vulkan_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(VULKAN_LIBRARY
  NAMES
    vulkan
  HINTS
    ${_vulkan_SEARCH_DIRS}
  PATH_SUFFIXES
    lib
)

# handle the QUIETLY and REQUIRED arguments and set VULKAN_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vulkan DEFAULT_MSG VULKAN_LIBRARY VULKAN_INCLUDE_DIR)

if(VULKAN_FOUND)
  set(VULKAN_LIBRARIES ${VULKAN_LIBRARY})
  set(VULKAN_INCLUDE_DIRS ${VULKAN_INCLUDE_DIR})
endif()

mark_as_advanced(
  VULKAN_INCLUDE_DIR
  VULKAN_LIBRARY
)

unset(_vulkan_SEARCH_DIRS)
