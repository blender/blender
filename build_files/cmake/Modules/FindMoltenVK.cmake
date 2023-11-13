# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

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

# If `MOLTENVK_ROOT_DIR` was defined in the environment, use it.
if(DEFINED MOLTENVK_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{MOLTENVK_ROOT_DIR})
  set(MOLTENVK_ROOT_DIR $ENV{MOLTENVK_ROOT_DIR})
else()
  set(MOLTENVK_ROOT_DIR "")
endif()

set(_moltenvk_SEARCH_DIRS
  ${MOLTENVK_ROOT_DIR}
)

# FIXME: These finder modules typically don't use LIBDIR,
# this should be set by `./build_files/cmake/platform/` instead.
if(DEFINED LIBDIR)
  set(_moltenvk_SEARCH_DIRS ${_moltenvk_SEARCH_DIRS} ${LIBDIR}/moltenvk)
endif()

find_path(MOLTENVK_INCLUDE_DIR
  NAMES
    MoltenVK/vk_mvk_moltenvk.h
  HINTS
    ${_moltenvk_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(MOLTENVK_LIBRARY
  NAMES
    MoltenVK
  HINTS
    ${_moltenvk_SEARCH_DIRS}
  PATH_SUFFIXES
    dylib/macOS
)

# handle the QUIETLY and REQUIRED arguments and set MOLTENVK_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MoltenVK DEFAULT_MSG MOLTENVK_LIBRARY MOLTENVK_INCLUDE_DIR)

if(MOLTENVK_FOUND)
  set(MOLTENVK_LIBRARIES ${MOLTENVK_LIBRARY})
  set(MOLTENVK_INCLUDE_DIRS ${MOLTENVK_INCLUDE_DIR})
endif()

mark_as_advanced(
  MOLTENVK_INCLUDE_DIR
  MOLTENVK_LIBRARY
)

unset(_moltenvk_SEARCH_DIRS)
