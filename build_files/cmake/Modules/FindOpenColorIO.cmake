# SPDX-FileCopyrightText: 2012 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find OpenColorIO library
# Find the native OpenColorIO includes and library
# This module defines
#  OPENCOLORIO_INCLUDE_DIRS, where to find OpenColorIO.h, Set when
#                            OPENCOLORIO_INCLUDE_DIR is found.
#  OPENCOLORIO_LIBRARIES, libraries to link against to use OpenColorIO.
#  OPENCOLORIO_ROOT_DIR, The base directory to search for OpenColorIO.
#                        This can also be an environment variable.
#  OPENCOLORIO_FOUND, If false, do not try to use OpenColorIO.
#
# also defined, but not for general use are
#  OPENCOLORIO_LIBRARY, where to find the OpenColorIO library.

# If `OPENCOLORIO_ROOT_DIR` was defined in the environment, use it.
if(DEFINED OPENCOLORIO_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{OPENCOLORIO_ROOT_DIR})
  set(OPENCOLORIO_ROOT_DIR $ENV{OPENCOLORIO_ROOT_DIR})
else()
  set(OPENCOLORIO_ROOT_DIR "")
endif()

set(_opencolorio_FIND_COMPONENTS
  OpenColorIO
  yaml-cpp
  expat
  pystring
  minizip
)

set(_opencolorio_SEARCH_DIRS
  ${OPENCOLORIO_ROOT_DIR}
  /opt/lib/ocio
)

find_path(OPENCOLORIO_INCLUDE_DIR
  NAMES
    OpenColorIO/OpenColorIO.h
  HINTS
    ${_opencolorio_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

set(_opencolorio_LIBRARIES)
foreach(COMPONENT ${_opencolorio_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  find_library(OPENCOLORIO_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_opencolorio_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib lib64/static lib/static
    )
  if(OPENCOLORIO_${UPPERCOMPONENT}_LIBRARY)
    list(APPEND _opencolorio_LIBRARIES "${OPENCOLORIO_${UPPERCOMPONENT}_LIBRARY}")
  endif()
endforeach()

if(EXISTS "${OPENCOLORIO_INCLUDE_DIR}/OpenColorIO/OpenColorABI.h")
  # Search twice, because this symbol changed between OCIO 1.x and 2.x
  file(STRINGS "${OPENCOLORIO_INCLUDE_DIR}/OpenColorIO/OpenColorABI.h" _opencolorio_version
    REGEX "^#define OCIO_VERSION_STR[ \t].*$")
  if(NOT _opencolorio_version)
    file(STRINGS "${OPENCOLORIO_INCLUDE_DIR}/OpenColorIO/OpenColorABI.h" _opencolorio_version
      REGEX "^#define OCIO_VERSION[ \t].*$")
  endif()
  string(REGEX MATCHALL "[0-9]+[.0-9]+" OPENCOLORIO_VERSION ${_opencolorio_version})
  unset(_opencolorio_version)
endif()

# handle the QUIETLY and REQUIRED arguments and set OPENCOLORIO_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenColorIO
    REQUIRED_VARS _opencolorio_LIBRARIES OPENCOLORIO_INCLUDE_DIR
    VERSION_VAR OPENCOLORIO_VERSION)

if(OPENCOLORIO_FOUND)
  set(OPENCOLORIO_LIBRARIES ${_opencolorio_LIBRARIES})
  set(OPENCOLORIO_INCLUDE_DIRS ${OPENCOLORIO_INCLUDE_DIR})
endif()

mark_as_advanced(
  OPENCOLORIO_INCLUDE_DIR
  OPENCOLORIO_LIBRARY
  OPENCOLORIO_VERSION
)

foreach(COMPONENT ${_opencolorio_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  mark_as_advanced(OPENCOLORIO_${UPPERCOMPONENT}_LIBRARY)
endforeach()

unset(COMPONENT)
unset(UPPERCOMPONENT)
unset(_opencolorio_FIND_COMPONENTS)
unset(_opencolorio_LIBRARIES)
unset(_opencolorio_SEARCH_DIRS)
