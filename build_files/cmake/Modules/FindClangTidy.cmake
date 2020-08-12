# - Find clang-tidy executable
#
# Find the native clang-tidy executable
#
# This module defines
#  CLANG_TIDY_EXECUTABLE, the ful lpath to clang-tidy executable
#
#  CLANG_TIDY_VERSION, the full version of the clang-tidy in the
#                      major,minor.patch format
#
# CLANG_TIDY_VERSION_MAJOR,
# CLANG_TIDY_VERSION_MINOR,
# CLANG_TIDY_VERSION_PATCH, individual components of the clang-tidy version.
#
#  CLANG_TIDY_FOUND, If false, do not try to use Eigen3.

#=============================================================================
# Copyright 2020 Blender Foundation.
#
# Distributed under the OSI-approved BSD 3-Clause License,
# see accompanying file BSD-3-Clause-license.txt for details.
#=============================================================================

# If CLANG_TIDY_ROOT_DIR was defined in the environment, use it.
if(NOT CLANG_TIDY_ROOT_DIR AND NOT $ENV{CLANG_TIDY_ROOT_DIR} STREQUAL "")
  set(CLANG_TIDY_ROOT_DIR $ENV{CLANG_TIDY_ROOT_DIR})
endif()

set(_clang_tidy_SEARCH_DIRS
  ${CLANG_TIDY_ROOT_DIR}
  /usr/local/bin
)

# TODO(sergey): Find more reliable way of finding the latest clang-tidy.
find_program(CLANG_TIDY_EXECUTABLE
  NAMES
    clang-tidy-10
    clang-tidy-9
    clang-tidy-8
    clang-tidy-7
    clang-tidy
  HINTS
    ${_clang_tidy_SEARCH_DIRS}
)

if(CLANG_TIDY_EXECUTABLE)
  # Mark clang-tidy as found.
  set(CLANG_TIDY_FOUND TRUE)

  # Setup fallback values.
  set(CLANG_TIDY_VERSION_MAJOR 0)
  set(CLANG_TIDY_VERSION_MINOR 0)
  set(CLANG_TIDY_VERSION_PATCH 0)

  # Get version from the output.
  #
  # NOTE: Don't use name of the executable file since that only includes a
  # major version. Also, even the major version might be missing in the
  # executable name.
  execute_process(COMMAND ${CLANG_TIDY_EXECUTABLE} -version
                  OUTPUT_VARIABLE CLANG_TIDY_VERSION_RAW
                  ERROR_QUIET
                  OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Parse parts.
  if(CLANG_TIDY_VERSION_RAW MATCHES "LLVM version .*")
    # Strip the LLVM prefix and get list of individual version components.
    string(REGEX REPLACE
           ".*LLVM version ([.0-9]+).*" "\\1"
           CLANG_SEMANTIC_VERSION "${CLANG_TIDY_VERSION_RAW}")
    string(REPLACE "." ";" CLANG_VERSION_PARTS "${CLANG_SEMANTIC_VERSION}")
    list(LENGTH CLANG_VERSION_PARTS NUM_CLANG_TIDY_VERSION_PARTS)

    # Extract components into corresponding variables.
    if(NUM_CLANG_TIDY_VERSION_PARTS GREATER 0)
      list(GET CLANG_VERSION_PARTS 0 CLANG_TIDY_VERSION_MAJOR)
    endif()
    if(NUM_CLANG_TIDY_VERSION_PARTS GREATER 1)
      list(GET CLANG_VERSION_PARTS 1 CLANG_TIDY_VERSION_MINOR)
    endif()
    if(NUM_CLANG_TIDY_VERSION_PARTS GREATER 2)
      list(GET CLANG_VERSION_PARTS 2 CLANG_TIDY_VERSION_PATCH)
    endif()

    # Unset temp variables.
    unset(NUM_CLANG_TIDY_VERSION_PARTS)
    unset(CLANG_SEMANTIC_VERSION)
    unset(CLANG_VERSION_PARTS)
  endif()

  # Construct full semantic version.
  set(CLANG_TIDY_VERSION "${CLANG_TIDY_VERSION_MAJOR}.\
${CLANG_TIDY_VERSION_MINOR}.\
${CLANG_TIDY_VERSION_PATCH}")
  unset(CLANG_TIDY_VERSION_RAW)

  message(STATUS "Found clang-tidy ${CLANG_TIDY_EXECUTABLE} (${CLANG_TIDY_VERSION})")
else()
  set(CLANG_TIDY_FOUND FALSE)
endif()
