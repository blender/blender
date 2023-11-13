# SPDX-FileCopyrightText: 2015 Google Inc. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause

# Ceres Solver - A fast non-linear least squares minimizer http://ceres-solver.org/
# Author: Alex Stewart <alexs.mac@gmail.com>

# FindGlog.cmake - Find Google glog logging library.
#
# This module defines the following variables:
#
# GLOG_FOUND: TRUE iff glog is found.
# GLOG_INCLUDE_DIRS: Include directories for glog.
# GLOG_LIBRARIES: Libraries required to link glog.
#
# The following variables control the behaviour of this module:
#
# GLOG_INCLUDE_DIR_HINTS: List of additional directories in which to
#                         search for glog includes, e.g: /timbuktu/include.
# GLOG_LIBRARY_DIR_HINTS: List of additional directories in which to
#                         search for glog libraries, e.g: /timbuktu/lib.
# GFLOG_ROOT_DIR,         The base directory to search for Glog.
#                         This can also be an environment variable.
#
# The following variables are also defined by this module, but in line with
# CMake recommended FindPackage() module style should NOT be referenced directly
# by callers (use the plural variables detailed above instead).  These variables
# do however affect the behaviour of the module via FIND_[PATH/LIBRARY]() which
# are NOT re-called (i.e. search for library is not repeated) if these variables
# are set with valid values _in the CMake cache_. This means that if these
# variables are set directly in the cache, either by the user in the CMake GUI,
# or by the user passing -DVAR=VALUE directives to CMake when called (which
# explicitly defines a cache variable), then they will be used verbatim,
# bypassing the HINTS variables and other hard-coded search locations.
#
# GLOG_INCLUDE_DIR: Include directory for glog, not including the
#                   include directory of any dependencies.
# GLOG_LIBRARY: glog library, not including the libraries of any
#               dependencies.

# If `GLOG_ROOT_DIR` was defined in the environment, use it.
if(DEFINED GLOG_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{GLOG_ROOT_DIR})
  set(GLOG_ROOT_DIR $ENV{GLOG_ROOT_DIR})
else()
  set(GLOG_ROOT_DIR "")
endif()

if(DEFINED GLOG_ROOT_DIR)
  set(GLOG_ROOT_DIR_INCLUDE "${GLOG_ROOT_DIR}/include")
  set(GLOG_ROOT_DIR_LIB "${GLOG_ROOT_DIR}/lib")
endif()

# Reset CALLERS_CMAKE_FIND_LIBRARY_PREFIXES to its value when
# FindGlog was invoked.
macro(GLOG_RESET_FIND_LIBRARY_PREFIX)
  if(MSVC)
    set(CMAKE_FIND_LIBRARY_PREFIXES "${CALLERS_CMAKE_FIND_LIBRARY_PREFIXES}")
  endif()
endmacro()

# Called if we failed to find glog or any of it's required dependencies,
# unsets all public (designed to be used externally) variables and reports
# error message at priority depending upon [REQUIRED/QUIET/<NONE>] argument.
macro(GLOG_REPORT_NOT_FOUND REASON_MSG)
  unset(GLOG_FOUND)
  unset(GLOG_INCLUDE_DIRS)
  unset(GLOG_LIBRARIES)
  # Make results of search visible in the CMake GUI if glog has not
  # been found so that user does not have to toggle to advanced view.
  mark_as_advanced(CLEAR GLOG_INCLUDE_DIR
                         GLOG_LIBRARY)

  glog_reset_find_library_prefix()

  # Note <package>_FIND_[REQUIRED/QUIETLY] variables defined by FindPackage()
  # use the camelcase library name, not uppercase.
  if(Glog_FIND_QUIETLY)
    message(STATUS "Failed to find glog - " ${REASON_MSG} ${ARGN})
  elseif(Glog_FIND_REQUIRED)
    message(FATAL_ERROR "Failed to find glog - " ${REASON_MSG} ${ARGN})
  else()
    # Neither QUIETLY nor REQUIRED, use no priority which emits a message
    # but continues configuration and allows generation.
    message("-- Failed to find glog - " ${REASON_MSG} ${ARGN})
  endif()
  return()
endmacro()

# Handle possible presence of lib prefix for libraries on MSVC, see
# also GLOG_RESET_FIND_LIBRARY_PREFIX().
if(MSVC)
  # Preserve the caller's original values for CMAKE_FIND_LIBRARY_PREFIXES
  # s/t we can set it back before returning.
  set(CALLERS_CMAKE_FIND_LIBRARY_PREFIXES "${CMAKE_FIND_LIBRARY_PREFIXES}")
  # The empty string in this list is important, it represents the case when
  # the libraries have no prefix (shared libraries / DLLs).
  set(CMAKE_FIND_LIBRARY_PREFIXES "lib" "" "${CMAKE_FIND_LIBRARY_PREFIXES}")
endif()

# Search user-installed locations first, so that we prefer user installs
# to system installs where both exist.
list(APPEND GLOG_CHECK_INCLUDE_DIRS
  ${GLOG_ROOT_DIR_INCLUDE}
  /usr/local/include
  /usr/local/homebrew/include # Mac OS X
  /opt/local/var/macports/software # Mac OS X.
  /opt/local/include
  /usr/include
  /opt/lib/glog/include)
# Windows (for C:/Program Files prefix).
list(APPEND GLOG_CHECK_PATH_SUFFIXES
  glog/include
  glog/Include
  Glog/include
  Glog/Include)

list(APPEND GLOG_CHECK_LIBRARY_DIRS
  ${GLOG_ROOT_DIR_LIB}
  /usr/local/lib
  /usr/local/homebrew/lib # Mac OS X.
  /opt/local/lib
  /usr/lib
  /opt/lib/gflags/lib)
# Windows (for C:/Program Files prefix).
list(APPEND GLOG_CHECK_LIBRARY_SUFFIXES
  glog/lib
  glog/Lib
  Glog/lib
  Glog/Lib)

# Search supplied hint directories first if supplied.
find_path(GLOG_INCLUDE_DIR
  NAMES glog/logging.h
  PATHS ${GLOG_INCLUDE_DIR_HINTS}
  ${GLOG_CHECK_INCLUDE_DIRS}
  PATH_SUFFIXES ${GLOG_CHECK_PATH_SUFFIXES})
if(NOT GLOG_INCLUDE_DIR OR
   NOT EXISTS ${GLOG_INCLUDE_DIR})
  glog_report_not_found(
    "Could not find glog include directory, set GLOG_INCLUDE_DIR "
    "to directory containing glog/logging.h")
endif()

find_library(GLOG_LIBRARY NAMES glog
  PATHS ${GLOG_LIBRARY_DIR_HINTS}
  ${GLOG_CHECK_LIBRARY_DIRS}
  PATH_SUFFIXES ${GLOG_CHECK_LIBRARY_SUFFIXES})
if(NOT GLOG_LIBRARY OR
   NOT EXISTS ${GLOG_LIBRARY})
  glog_report_not_found(
    "Could not find glog library, set GLOG_LIBRARY "
    "to full path to libglog.")
endif()

# Mark internally as found, then verify. GLOG_REPORT_NOT_FOUND() unsets
# if called.
set(GLOG_FOUND TRUE)

# Glog does not seem to provide any record of the version in its
# source tree, thus cannot extract version.

# Catch case when caller has set GLOG_INCLUDE_DIR in the cache / GUI and
# thus FIND_[PATH/LIBRARY] are not called, but specified locations are
# invalid, otherwise we would report the library as found.
if(GLOG_INCLUDE_DIR AND
   NOT EXISTS ${GLOG_INCLUDE_DIR}/glog/logging.h)
  glog_report_not_found(
    "Caller defined GLOG_INCLUDE_DIR:"
    " ${GLOG_INCLUDE_DIR} does not contain glog/logging.h header.")
endif()
# TODO: This regex for glog library is pretty primitive, we use lowercase
#       for comparison to handle Windows using CamelCase library names, could
#       this check be better?
string(TOLOWER "${GLOG_LIBRARY}" LOWERCASE_GLOG_LIBRARY)
if(GLOG_LIBRARY AND
   NOT "${LOWERCASE_GLOG_LIBRARY}" MATCHES ".*glog[^/]*")
  glog_report_not_found(
    "Caller defined GLOG_LIBRARY: "
    "${GLOG_LIBRARY} does not match glog.")
endif()

# Set standard CMake FindPackage variables if found.
if(GLOG_FOUND)
  set(GLOG_INCLUDE_DIRS ${GLOG_INCLUDE_DIR})
  set(GLOG_LIBRARIES ${GLOG_LIBRARY})
endif()

glog_reset_find_library_prefix()

# Handle REQUIRED / QUIET optional arguments.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLOG DEFAULT_MSG
  GLOG_INCLUDE_DIRS GLOG_LIBRARIES)

# Only mark internal variables as advanced if we found glog, otherwise
# leave them visible in the standard GUI for the user to set manually.
if(GLOG_FOUND)
  mark_as_advanced(FORCE GLOG_INCLUDE_DIR
                         GLOG_LIBRARY)
endif()
