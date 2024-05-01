# SPDX-FileCopyrightText: 2021 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# Find HIP compiler. This module defines
#  HIP_HIPCC_EXECUTABLE, the full path to the hipcc executable
#  HIP_VERSION, the HIP compiler version
#  HIP_FOUND, if the HIP toolkit is found.

if(NOT (DEFINED HIP_ROOT_DIR))
  set(HIP_ROOT_DIR "")
endif()

# If `HIP_ROOT_DIR` was defined in the environment, use it.
if(HIP_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{HIP_ROOT_DIR})
  set(HIP_ROOT_DIR $ENV{HIP_ROOT_DIR})
elseif(DEFINED ENV{HIP_PATH})
  # Built-in environment variable from SDK.
  set(HIP_ROOT_DIR $ENV{HIP_PATH})
endif()

set(_hip_SEARCH_DIRS
  ${HIP_ROOT_DIR}
)

find_program(HIP_HIPCC_EXECUTABLE
  NAMES
    hipcc
  HINTS
    ${_hip_SEARCH_DIRS}
  PATH_SUFFIXES
    bin
)

if(WIN32)
  # Needed for HIP-RT on Windows.
  find_program(HIP_LINKER_EXECUTABLE
    NAMES
      clang++
    HINTS
      ${_hip_SEARCH_DIRS}
    PATH_SUFFIXES
      bin
    NO_DEFAULT_PATH
    NO_CMAKE_PATH
  )
endif()

if(HIP_HIPCC_EXECUTABLE)
  set(HIP_VERSION_MAJOR 0)
  set(HIP_VERSION_MINOR 0)
  set(HIP_VERSION_PATCH 0)

  if(WIN32)
    set(_hipcc_executable ${HIP_HIPCC_EXECUTABLE}.bat)
  else()
    set(_hipcc_executable ${HIP_HIPCC_EXECUTABLE})
  endif()

  # Get version from the output.
  execute_process(COMMAND ${_hipcc_executable} --version
                  OUTPUT_VARIABLE _hip_version_raw
                  ERROR_QUIET
                  OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Parse parts.
  if(_hip_version_raw MATCHES "HIP version: .*")
    # Strip the HIP prefix and get list of individual version components.
    string(REGEX REPLACE
           ".*HIP version: ([.0-9]+).*" "\\1"
           _hip_semantic_version "${_hip_version_raw}")
    string(REPLACE "." ";" _hip_version_parts "${_hip_semantic_version}")
    list(LENGTH _hip_version_parts _num_hip_version_parts)

    # Extract components into corresponding variables.
    if(_num_hip_version_parts GREATER 0)
      list(GET _hip_version_parts 0 HIP_VERSION_MAJOR)
    endif()
    if(_num_hip_version_parts GREATER 1)
      list(GET _hip_version_parts 1 HIP_VERSION_MINOR)
    endif()
    if(_num_hip_version_parts GREATER 2)
      list(GET _hip_version_parts 2 HIP_VERSION_PATCH)
    endif()

    # Unset temp variables.
    unset(_num_hip_version_parts)
    unset(_hip_semantic_version)
    unset(_hip_version_parts)
  endif()

  # Construct full semantic version.
  set(HIP_VERSION "${HIP_VERSION_MAJOR}.${HIP_VERSION_MINOR}.${HIP_VERSION_PATCH}")
  unset(_hip_version_raw)
  unset(_hipcc_executable)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HIP
    REQUIRED_VARS HIP_HIPCC_EXECUTABLE
    VERSION_VAR HIP_VERSION)

unset(_hip_SEARCH_DIRS)
