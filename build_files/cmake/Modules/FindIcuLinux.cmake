# SPDX-FileCopyrightText: 2012 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find static icu libraries
# Find the native static icu libraries (needed for static boost_locale :/ ).
# This module defines
#  ICU_LIBRARIES, libraries to link against to use icu.
#  ICU_ROOT_DIR, The base directory to search for icu.
#                    This can also be an environment variable.
#  ICU_FOUND, If false, do not try to use icu.
#
# also defined, but not for general use are
#  ICU_LIBRARY_xxx, where to find the icu libraries.

# If `ICU_ROOT_DIR` was defined in the environment, use it.
if(DEFINED ICU_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{ICU_ROOT_DIR})
  set(ICU_ROOT_DIR $ENV{ICU_ROOT_DIR})
else()
  set(ICU_ROOT_DIR "")
endif()

if(Boost_USE_STATIC_LIBS)
  set(_icu_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()

set(_icu_SEARCH_DIRS
  ${ICU_ROOT_DIR}
)

# We don't need includes, only libs to link against...
# find_path(ICU_INCLUDE_DIR
#   NAMES
#     utf.h
#   HINTS
#     ${_icu_SEARCH_DIRS}
#   PATH_SUFFIXES
#     include/unicode
# )

find_library(ICU_LIBRARY_DATA
  NAMES
    icudata
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

find_library(ICU_LIBRARY_I18N
  NAMES
    icui18n
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

find_library(ICU_LIBRARY_IO
  NAMES
    icuio
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

find_library(ICU_LIBRARY_LE
  NAMES
    icule
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

find_library(ICU_LIBRARY_LX
  NAMES
    iculx
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

find_library(ICU_LIBRARY_TU
  NAMES
    icutu
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

find_library(ICU_LIBRARY_UC
  NAMES
    icuuc
  HINTS
    ${_icu_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# Restore the original find library ordering
if(Boost_USE_STATIC_LIBS)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${_icu_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

# handle the QUIETLY and REQUIRED arguments and set ICU_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Icu DEFAULT_MSG
    ICU_LIBRARY_DATA
    ICU_LIBRARY_I18N
    ICU_LIBRARY_IO
    ICU_LIBRARY_LE
    ICU_LIBRARY_LX
    ICU_LIBRARY_TU
    ICU_LIBRARY_UC
)

if(ICU_FOUND)
  set(ICU_LIBRARIES ${ICU_LIBRARY_DATA} ${ICU_LIBRARY_I18N} ${ICU_LIBRARY_IO} ${ICU_LIBRARY_LE} ${ICU_LIBRARY_LX} ${ICU_LIBRARY_TU} ${ICU_LIBRARY_UC})
  set(ICU_INCLUDE_DIRS ${ICU_INCLUDE_DIR})
endif()

mark_as_advanced(
  ICU_INCLUDE_DIR
  ICU_LIBRARY_DATA
  ICU_LIBRARY_I18N
  ICU_LIBRARY_IO
  ICU_LIBRARY_LE
  ICU_LIBRARY_LX
  ICU_LIBRARY_TU
  ICU_LIBRARY_UC
)

unset(_icu_SEARCH_DIRS)
