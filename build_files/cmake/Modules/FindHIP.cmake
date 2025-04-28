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
  /opt/rocm
  /opt/rocm/hip
  "C:/Program Files/AMD/ROCm/*"
)

find_program(HIP_HIPCC_EXECUTABLE
  NAMES
    hipcc
  HINTS
    ${_hip_SEARCH_DIRS}
  PATH_SUFFIXES
    bin
)

if(HIP_HIPCC_EXECUTABLE)
  if(NOT HIP_ROOT_DIR)
    get_filename_component(HIP_ROOT_DIR ${HIP_HIPCC_EXECUTABLE} DIRECTORY)
    get_filename_component(HIP_ROOT_DIR ${HIP_ROOT_DIR} DIRECTORY)
  endif()

  set(HIP_VERSION_MAJOR 0)
  set(HIP_VERSION_MINOR 0)
  set(HIP_VERSION_PATCH 0)

  # Get version from the header.
  set(_hip_version_file "${HIP_ROOT_DIR}/include/hip/hip_version.h")
  if(EXISTS ${_hip_version_file})
    file(STRINGS ${_hip_version_file} _tmp REGEX "^#define HIP_VERSION_MAJOR.*$")
    string(REGEX MATCHALL "[0-9]+" HIP_VERSION_MAJOR ${_tmp})
    file(STRINGS ${_hip_version_file} _tmp REGEX "^#define HIP_VERSION_MINOR.*$")
    string(REGEX MATCHALL "[0-9]+" HIP_VERSION_MINOR ${_tmp})
    file(STRINGS ${_hip_version_file} _tmp REGEX "^#define HIP_VERSION_PATCH.*$")
    string(REGEX MATCHALL "[0-9]+" HIP_VERSION_PATCH ${_tmp})
    unset(_tmp)
  endif()
  unset(_hip_version_file)

  # Construct full semantic version.
  set(HIP_VERSION "${HIP_VERSION_MAJOR}.${HIP_VERSION_MINOR}.${HIP_VERSION_PATCH}")
  set(HIP_VERSION_SHORT "${HIP_VERSION_MAJOR}.${HIP_VERSION_MINOR}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HIP
  REQUIRED_VARS HIP_HIPCC_EXECUTABLE
  VERSION_VAR HIP_VERSION)

mark_as_advanced(
  HIP_HIPCC_EXECUTABLE
)

unset(_hip_SEARCH_DIRS)
