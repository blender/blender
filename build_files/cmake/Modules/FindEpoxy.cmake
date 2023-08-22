# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# This module defines
#  Epoxy_INCLUDE_DIRS, where to find epoxy/gl.h
#  Epoxy_LIBRARY, where to find the epoxy library.
#  Epoxy_ROOT_DIR, The base directory to search for epoxy.
#                     This can also be an environment variable.
#  Epoxy_FOUND, If false, do not try to use epoxy.

# If `EPOXY_ROOT_DIR` was defined in the environment, use it.
if(DEFINED EPOXY_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{EPOXY_ROOT_DIR})
  set(EPOXY_ROOT_DIR $ENV{EPOXY_ROOT_DIR})
else()
  set(EPOXY_ROOT_DIR "")
endif()

find_path(Epoxy_INCLUDE_DIR
  NAMES
    epoxy/gl.h
  HINTS
    ${EPOXY_ROOT_DIR}
  PATH_SUFFIXES
    include
)

find_library(Epoxy_LIBRARY
  NAMES
    epoxy
  HINTS
    ${EPOXY_ROOT_DIR}
  PATH_SUFFIXES
    lib64 lib
)

# handle the QUIETLY and REQUIRED arguments and set Epoxy_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Epoxy DEFAULT_MSG
    Epoxy_LIBRARY Epoxy_INCLUDE_DIR)

if(Epoxy_FOUND)
  set(Epoxy_INCLUDE_DIRS ${Epoxy_INCLUDE_DIR})
  set(Epoxy_LIBRARIES ${Epoxy_LIBRARY})
endif()

mark_as_advanced(
  Epoxy_INCLUDE_DIR
  Epoxy_LIBRARY
)
