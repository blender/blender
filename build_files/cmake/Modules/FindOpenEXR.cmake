# SPDX-FileCopyrightText: 2011 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find OpenEXR library
# Find the native OpenEXR includes and library
# This module defines
#  OPENEXR_INCLUDE_DIRS, where to find ImfXdr.h, etc. Set when
#                        OPENEXR_INCLUDE_DIR is found.
#  OPENEXR_LIBRARIES, libraries to link against to use OpenEXR.
#  OPENEXR_ROOT_DIR, The base directory to search for OpenEXR.
#                    This can also be an environment variable.
#  OPENEXR_FOUND, If false, do not try to use OpenEXR.
#
# For individual library access these advanced settings are available
#  OPENEXR_HALF_LIBRARY, Path to Half library
#  OPENEXR_IEX_LIBRARY, Path to Half library
#  OPENEXR_ILMIMF_LIBRARY, Path to Ilmimf library
#  OPENEXR_ILMTHREAD_LIBRARY, Path to IlmThread library
#  OPENEXR_IMATH_LIBRARY, Path to Imath library
#
# also defined, but not for general use are
#  OPENEXR_LIBRARY, where to find the OpenEXR library.

# If `OPENEXR_ROOT_DIR` was defined in the environment, use it.
if(DEFINED OPENEXR_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{OPENEXR_ROOT_DIR})
  set(OPENEXR_ROOT_DIR $ENV{OPENEXR_ROOT_DIR})
else()
  set(OPENEXR_ROOT_DIR "")
endif()

# Old versions (before 2.0?) do not have any version string,
# just assuming this should be fine though.
set(_openexr_libs_ver_init "2.0")

set(_openexr_SEARCH_DIRS
  ${OPENEXR_ROOT_DIR}
  /opt/lib/openexr
)

find_path(OPENEXR_INCLUDE_DIR
  NAMES
    OpenEXR/ImfXdr.h
  HINTS
    ${_openexr_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

# If the headers were found, get the version from config file, if not already set.
if(OPENEXR_INCLUDE_DIR)
  if(NOT OPENEXR_VERSION)

    find_file(_openexr_CONFIG
      NAMES
        OpenEXRConfig.h
      PATHS
        "${OPENEXR_INCLUDE_DIR}"
        "${OPENEXR_INCLUDE_DIR}/OpenEXR"
      NO_DEFAULT_PATH
    )

    if(_openexr_CONFIG)
      file(STRINGS "${_openexr_CONFIG}" OPENEXR_BUILD_SPECIFICATION
           REGEX "^[ \t]*#define[ \t]+OPENEXR_VERSION_STRING[ \t]+\"[.0-9]+\".*$")
    else()
      message(WARNING "Could not find \"OpenEXRConfig.h\" in \"${OPENEXR_INCLUDE_DIR}\"")
    endif()

    if(OPENEXR_BUILD_SPECIFICATION)
      message(STATUS "${OPENEXR_BUILD_SPECIFICATION}")
      string(REGEX REPLACE ".*#define[ \t]+OPENEXR_VERSION_STRING[ \t]+\"([.0-9]+)\".*"
             "\\1" _openexr_libs_ver_init ${OPENEXR_BUILD_SPECIFICATION})
    else()
      message(WARNING "Could not determine ILMBase library version, assuming ${_openexr_libs_ver_init}.")
    endif()

    unset(_openexr_CONFIG CACHE)

  endif()
endif()

set("OPENEXR_VERSION" ${_openexr_libs_ver_init} CACHE STRING "Version of OpenEXR lib")
unset(_openexr_libs_ver_init)

string(REGEX REPLACE "([0-9]+)[.]([0-9]+).*" "\\1_\\2" _openexr_libs_ver ${OPENEXR_VERSION})

# Different library names in 3.0, and Imath and Half moved out.
if(OPENEXR_VERSION VERSION_GREATER_EQUAL "3.0.0")
  set(_openexr_FIND_COMPONENTS
    Iex
    OpenEXR
    OpenEXRCore
    IlmThread
  )
else()
  set(_openexr_FIND_COMPONENTS
    Half
    Iex
    IlmImf
    IlmThread
    Imath
  )
endif()

set(_openexr_LIBRARIES)
foreach(COMPONENT ${_openexr_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  find_library(OPENEXR_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}-${_openexr_libs_ver} ${COMPONENT}
    NAMES_PER_DIR
    HINTS
      ${_openexr_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  list(APPEND _openexr_LIBRARIES "${OPENEXR_${UPPERCOMPONENT}_LIBRARY}")
endforeach()

unset(_openexr_libs_ver)

if(OPENEXR_VERSION VERSION_GREATER_EQUAL "3.0.0")
  # For OpenEXR 3.x, we also need to find the now separate Imath library.
  # For simplicity we also add it to the OpenEXR includes and libraries,
  # as it's simpler to support both 2.x and 3.x this way.

  # Find include directory
  find_path(IMATH_INCLUDE_DIR
    NAMES
      Imath/ImathMath.h
    HINTS
      ${_openexr_SEARCH_DIRS}
    PATH_SUFFIXES
      include
  )

  # Find version
  find_file(_imath_config
    NAMES
      ImathConfig.h
    PATHS
      ${IMATH_INCLUDE_DIR}/Imath
    NO_DEFAULT_PATH
  )

  # Find line with version, extract string, and format for library suffix.
  file(STRINGS "${_imath_config}" _imath_build_specification
       REGEX "^[ \t]*#define[ \t]+IMATH_VERSION_STRING[ \t]+\"[.0-9]+\".*$")
  string(REGEX REPLACE ".*#define[ \t]+IMATH_VERSION_STRING[ \t]+\"([.0-9]+)\".*"
         "\\1" _imath_libs_ver ${_imath_build_specification})
  string(REGEX REPLACE "([0-9]+)[.]([0-9]+).*" "\\1_\\2" _imath_libs_ver ${_imath_libs_ver})

  # Find library, with or without version number.
  find_library(IMATH_LIBRARY
    NAMES
      Imath-${_imath_libs_ver} Imath
    NAMES_PER_DIR
    HINTS
      ${_openexr_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  list(APPEND _openexr_LIBRARIES "${IMATH_LIBRARY}")

  # In cmake version 3.21 and up, we can instead use the NO_CACHE option for
  # FIND_FILE so we don't need to clear it from the cache here.
  unset(_imath_config CACHE)
  unset(_imath_libs_ver)
  unset(_imath_build_specification)
endif()

if(OPENEXR_VERSION VERSION_GREATER_EQUAL "3.0.0")
  set(IMATH_LIBRARIES ${IMATH_LIBRARY})
else()
  set(IMATH_LIBRARIES ${OPENEXR_IMATH_LIBRARY})
endif()

# handle the QUIETLY and REQUIRED arguments and set OPENEXR_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenEXR DEFAULT_MSG
    _openexr_LIBRARIES OPENEXR_INCLUDE_DIR)

if(OPENEXR_FOUND)
  set(OPENEXR_LIBRARIES ${_openexr_LIBRARIES})
  # Both include paths are needed because of dummy OSL headers mixing
  # #include <OpenEXR/foo.h> and #include <foo.h>, as well as Alembic
  # include <half.h> directly.
  set(OPENEXR_INCLUDE_DIRS
    ${OPENEXR_INCLUDE_DIR}
    ${OPENEXR_INCLUDE_DIR}/OpenEXR)

  if(OPENEXR_VERSION VERSION_GREATER_EQUAL "3.0.0")
    list(APPEND OPENEXR_INCLUDE_DIRS
      ${IMATH_INCLUDE_DIR}
      ${IMATH_INCLUDE_DIR}/Imath)
  endif()

  set(IMATH_INCLUDE_DIRS
    ${IMATH_INCLUDE_DIR})
endif()

mark_as_advanced(
  OPENEXR_INCLUDE_DIR
  OPENEXR_VERSION
  IMATH_INCLUDE_DIR
  IMATH_LIBRARY
  IMATH_LIBRARIES
)
foreach(COMPONENT ${_openexr_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  mark_as_advanced(OPENEXR_${UPPERCOMPONENT}_LIBRARY)
endforeach()

unset(COMPONENT)
unset(UPPERCOMPONENT)
unset(_openexr_FIND_COMPONENTS)
unset(_openexr_LIBRARIES)
unset(_openexr_SEARCH_DIRS)
