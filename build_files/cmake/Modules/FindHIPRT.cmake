# SPDX-FileCopyrightText: 2021 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# Find HIPRT SDK. This module defines:
#   HIPRT_INCLUDE_DIRS, path to HIPRT include directory
#   HIPRT_LIBRARIES, libraries to link against to use HIPRT.
#   HIPRT_VERSION, the HIPRT library version string.
#   HIPRT_FOUND, if SDK found
#
# also defined, but not for general use are
#   HIPRT_INCLUDE_DIR, path to HIPRT include directory
#   HIPRT_LIBRARY, where to find the HIPRT library.

if(NOT (DEFINED HIPRT_ROOT_DIR))
  set(HIPRT_ROOT_DIR "")
endif()

# If `HIPRT_ROOT_DIR` was defined in the environment, use it.
if(HIPRT_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{HIPRT_ROOT_DIR})
  set(HIPRT_ROOT_DIR $ENV{HIPRT_ROOT_DIR})
elseif(DEFINED ENV{HIP_PATH})
  # Built-in environment variable from SDK.
  set(HIPRT_ROOT_DIR $ENV{HIP_PATH})
endif()

set(_hiprt_SEARCH_DIRS
  ${HIPRT_ROOT_DIR}
  /opt/lib/hiprt
)

find_path(HIPRT_INCLUDE_DIR
  NAMES
    hiprt/hiprt.h
  HINTS
    ${_hiprt_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

set(HIPRT_VERSION "")

if(HIPRT_INCLUDE_DIR)
  file(STRINGS "${HIPRT_INCLUDE_DIR}/hiprt/hiprt.h" _hiprt_version
    REGEX "^#define HIPRT_VERSION_STR[ \t]\".*\"$")
  string(REGEX MATCHALL "[0-9]+[.0-9]+" HIPRT_VERSION ${_hiprt_version})

  unset(_hiprt_version)
endif()

# Special code that side-steps annoyance of updating to a newer HIP-RT version: CMake caches the
# library path variable and it does include an API in the file path. Without any extra logic manual
# steps of updating local CMakeCache.txt would be needed.
# The code checks that the API of the cached variable is what it is expected to be based on the
# headers. If the API mismatches and the library exists skip the logic, assuming developers
# requested such configuration explicitly.
if(HIPRT_LIBRARY AND NOT EXISTS ${HIPRT_LIBRARY})
  get_filename_component(hiprt_library_name_we "${HIPRT_LIBRARY}" NAME_WE)
  # The file name is not guaranteed to have API in it. Check for it to explicitly set the API to an
  # empty string. Conditional group in the regex could help, but it is not trivial to address it in
  # the replacement string.
  if("${hiprt_library_name_we}" MATCHES "^(lib)?hiprt([0-9]+)64$")
    string(REGEX REPLACE "^(lib)?hiprt([0-9]+)64$" "\\2"
           hiprt_library_api "${hiprt_library_name_we}")
  else()
    set(hiprt_library_api "")
  endif()
  if(NOT "${HIPRT_VERSION}" STREQUAL "${hiprt_library_api}")
    message(STATUS "${HIPRT_LIBRARY} does not exist and has wrong API."
        "Forcing re-discovery."
    )
    unset(HIPRT_LIBRARY CACHE)
  endif()
  unset(hiprt_library_name_we)
  unset(hiprt_library_api)
endif()

find_library(HIPRT_LIBRARY
  NAMES
    hiprt64
    hiprt${HIPRT_VERSION}64
  HINTS
    ${_hiprt_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib bin
  # Skip CMake install prefix to prevent library from being found under
  # `bin/lib/libhiprt<api>64.so` in the build folder.
  NO_CMAKE_INSTALL_PREFIX
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HIPRT
  REQUIRED_VARS HIPRT_LIBRARY HIPRT_INCLUDE_DIR
  VERSION_VAR HIPRT_VERSION)

if(HIPRT_FOUND)
  set(HIPRT_LIBRARIES ${HIPRT_LIBRARY})
  set(HIPRT_INCLUDE_DIRS ${HIPRT_INCLUDE_DIR})
endif()

mark_as_advanced(
  HIPRT_INCLUDE_DIR
  HIPRT_LIBRARY
)

unset(_hiprt_SEARCH_DIRS)
