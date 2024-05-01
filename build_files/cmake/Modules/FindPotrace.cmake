# SPDX-FileCopyrightText: 2020 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find potrace library
# Find the potrace include and library
# This module defines
#  POTRACE_INCLUDE_DIRS, where to find potracelib.h, Set when
#                    POTRACE is found.
#  POTRACE_LIBRARIES, libraries to link against to use POTRACE.
#  POTRACE_ROOT_DIR, The base directory to search for POTRACE.
#                This can also be an environment variable.
#  POTRACE_FOUND, If false, do not try to use POTRACE.
#
# also defined, but not for general use are
#  POTRACE_LIBRARY, where to find the POTRACE library.

# If `POTRACE_ROOT_DIR` was defined in the environment, use it.
if(DEFINED POTRACE_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{POTRACE_ROOT_DIR})
  set(POTRACE_ROOT_DIR $ENV{POTRACE_ROOT_DIR})
else()
  set(POTRACE_ROOT_DIR "")
endif()

set(_potrace_SEARCH_DIRS
  ${POTRACE_ROOT_DIR}
  /opt/lib/potrace
  /usr/include
  /usr/local/include
)

find_path(POTRACE_INCLUDE_DIR
  NAMES
    potracelib.h
  HINTS
    ${_potrace_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(POTRACE_LIBRARY
  NAMES
    potrace
  HINTS
    ${_potrace_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set POTRACE_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Potrace DEFAULT_MSG
    POTRACE_LIBRARY POTRACE_INCLUDE_DIR)

if(POTRACE_FOUND)
  set(POTRACE_LIBRARIES ${POTRACE_LIBRARY})
  set(POTRACE_INCLUDE_DIRS ${POTRACE_INCLUDE_DIR})
endif()

mark_as_advanced(
  POTRACE_INCLUDE_DIR
  POTRACE_LIBRARY
)

unset(_potrace_SEARCH_DIRS)
