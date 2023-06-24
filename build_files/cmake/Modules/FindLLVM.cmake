# SPDX-FileCopyrightText: 2015 Blender Foundation
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find LLVM library
# Find the native LLVM includes and library
# This module defines
#  LLVM_INCLUDE_DIRS, where to find LLVM.h, Set when LLVM_INCLUDE_DIR is found.
#  LLVM_LIBRARIES, libraries to link against to use LLVM.
#  LLVM_ROOT_DIR, The base directory to search for LLVM.
#                This can also be an environment variable.
#  LLVM_FOUND, If false, do not try to use LLVM.
#
# also defined, but not for general use are
#  LLVM_LIBRARY, where to find the LLVM library.

if(LLVM_ROOT_DIR)
  if(DEFINED LLVM_VERSION)
    find_program(LLVM_CONFIG llvm-config-${LLVM_VERSION} HINTS ${LLVM_ROOT_DIR}/bin NO_CMAKE_PATH)
  endif()
  if(NOT LLVM_CONFIG)
    find_program(LLVM_CONFIG llvm-config HINTS ${LLVM_ROOT_DIR}/bin NO_CMAKE_PATH)
  endif()
else()
  if(DEFINED LLVM_VERSION)
    message(running llvm-config-${LLVM_VERSION})
    find_program(LLVM_CONFIG llvm-config-${LLVM_VERSION})
  endif()
  if(NOT LLVM_CONFIG)
    find_program(LLVM_CONFIG llvm-config)
  endif()
endif()

execute_process(COMMAND ${LLVM_CONFIG} --includedir
      OUTPUT_VARIABLE LLVM_INCLUDE_DIRS
      OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT DEFINED LLVM_VERSION)
  execute_process(COMMAND ${LLVM_CONFIG} --version
          OUTPUT_VARIABLE LLVM_VERSION
          OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(LLVM_VERSION ${LLVM_VERSION} CACHE STRING "Version of LLVM to use")
endif()
if(NOT LLVM_ROOT_DIR)
  execute_process(COMMAND ${LLVM_CONFIG} --prefix
          OUTPUT_VARIABLE LLVM_ROOT_DIR
          OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(LLVM_ROOT_DIR ${LLVM_ROOT_DIR} CACHE PATH "Path to the LLVM installation")
endif()
if(NOT LLVM_LIBPATH)
  execute_process(COMMAND ${LLVM_CONFIG} --libdir
          OUTPUT_VARIABLE LLVM_LIBPATH
          OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(LLVM_LIBPATH ${LLVM_LIBPATH} CACHE PATH "Path to the LLVM library path")
  mark_as_advanced(LLVM_LIBPATH)
endif()

if(LLVM_STATIC)
  find_library(LLVM_LIBRARY
               NAMES LLVMAnalysis # first of a whole bunch of libs to get
               PATHS ${LLVM_LIBPATH})
else()
  find_library(LLVM_LIBRARY
               NAMES
                 LLVM-${LLVM_VERSION}
                 LLVMAnalysis  # check for the static library as a fall-back
               PATHS ${LLVM_LIBPATH})
endif()


if(LLVM_LIBRARY AND LLVM_ROOT_DIR AND LLVM_LIBPATH)
  if(LLVM_STATIC)
    # if static LLVM libraries were requested, use llvm-config to generate
    # the list of what libraries we need, and substitute that in the right
    # way for LLVM_LIBRARY.
    execute_process(COMMAND ${LLVM_CONFIG} --libfiles
                    OUTPUT_VARIABLE LLVM_LIBRARY
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REPLACE " " ";" LLVM_LIBRARY "${LLVM_LIBRARY}")
  endif()
endif()


# handle the QUIETLY and REQUIRED arguments and set LLVM_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LLVM DEFAULT_MSG
    LLVM_LIBRARY)

MARK_AS_ADVANCED(
  LLVM_LIBRARY
)
