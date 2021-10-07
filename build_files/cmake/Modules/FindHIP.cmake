# - Find HIP compiler
#
# This module defines
#  HIP_HIPCC_EXECUTABLE, the full path to the hipcc executable
#  HIP_VERSION, the HIP compiler version
#
#  HIP_FOUND, if the HIP toolkit is found.

#=============================================================================
# Copyright 2021 Blender Foundation.
#
# Distributed under the OSI-approved BSD 3-Clause License,
# see accompanying file BSD-3-Clause-license.txt for details.
#=============================================================================

# If HIP_ROOT_DIR was defined in the environment, use it.
if(NOT HIP_ROOT_DIR AND NOT $ENV{HIP_ROOT_DIR} STREQUAL "")
  set(HIP_ROOT_DIR $ENV{HIP_ROOT_DIR})
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

if(HIP_HIPCC_EXECUTABLE AND NOT EXISTS ${HIP_HIPCC_EXECUTABLE})
  message(WARNING "Cached or directly specified hipcc executable does not exist.")
  set(HIP_FOUND FALSE)
elseif(HIP_HIPCC_EXECUTABLE)
  set(HIP_FOUND TRUE)

  set(HIP_VERSION_MAJOR 0)
  set(HIP_VERSION_MINOR 0)
  set(HIP_VERSION_PATCH 0)

  # Get version from the output.
  execute_process(COMMAND ${HIP_HIPCC_EXECUTABLE} --version
                  OUTPUT_VARIABLE HIP_VERSION_RAW
                  ERROR_QUIET
                  OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Parse parts.
  if(HIP_VERSION_RAW MATCHES "HIP version: .*")
    # Strip the HIP prefix and get list of individual version components.
    string(REGEX REPLACE
           ".*HIP version: ([.0-9]+).*" "\\1"
           HIP_SEMANTIC_VERSION "${HIP_VERSION_RAW}")
    string(REPLACE "." ";" HIP_VERSION_PARTS "${HIP_SEMANTIC_VERSION}")
    list(LENGTH HIP_VERSION_PARTS NUM_HIP_VERSION_PARTS)

    # Extract components into corresponding variables.
    if(NUM_HIP_VERSION_PARTS GREATER 0)
      list(GET HIP_VERSION_PARTS 0 HIP_VERSION_MAJOR)
    endif()
    if(NUM_HIP_VERSION_PARTS GREATER 1)
      list(GET HIP_VERSION_PARTS 1 HIP_VERSION_MINOR)
    endif()
    if(NUM_HIP_VERSION_PARTS GREATER 2)
      list(GET HIP_VERSION_PARTS 2 HIP_VERSION_PATCH)
    endif()

    # Unset temp variables.
    unset(NUM_HIP_VERSION_PARTS)
    unset(HIP_SEMANTIC_VERSION)
    unset(HIP_VERSION_PARTS)
  endif()

  # Construct full semantic version.
  set(HIP_VERSION "${HIP_VERSION_MAJOR}.${HIP_VERSION_MINOR}.${HIP_VERSION_PATCH}")
  unset(HIP_VERSION_RAW)
else()
  set(HIP_FOUND FALSE)
endif()
