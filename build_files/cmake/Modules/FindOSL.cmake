# SPDX-FileCopyrightText: 2014 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find OpenShadingLanguage library
# Find the native OpenShadingLanguage includes and library
# This module defines
#  OSL_INCLUDE_DIRS, where to find OSL headers, Set when
#                    OSL_INCLUDE_DIR is found.
#  OSL_LIBRARIES, libraries to link against to use OSL.
#  OSL_ROOT_DIR, the base directory to search for OSL.
#                This can also be an environment variable.
#  OSL_COMPILER, full path to OSL script compiler.
#  OSL_FOUND, if false, do not try to use OSL.
#  OSL_LIBRARY_VERSION_MAJOR, OSL_LIBRARY_VERSION_MINOR,  the major
#                and minor versions of OSL library if found.

# If `OSL_ROOT_DIR` was defined in the environment, use it.
if(DEFINED OSL_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{OSL_ROOT_DIR})
  set(OSL_ROOT_DIR $ENV{OSL_ROOT_DIR})
else()
  set(OSL_ROOT_DIR "")
endif()

# If `OSLHOME` was defined in the environment, use it.
if(DEFINED ENV{OSLHOME})
  set(OSL_HOME_DIR $ENV{OSLHOME})
else()
  set(OSL_HOME_DIR "")
endif()

set(_osl_FIND_COMPONENTS
  oslnoise
  oslcomp
  oslexec
  oslquery
)

set(_osl_SEARCH_DIRS
  ${OSL_ROOT_DIR}
  /opt/lib/osl
)

find_path(OSL_INCLUDE_DIR
  NAMES
    OSL/oslversion.h
  HINTS
    ${_osl_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

foreach(COMPONENT ${_osl_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  find_library(OSL_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_osl_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
endforeach()

# Note linking order matters, and oslnoise existence depends on version.
set(_osl_LIBRARIES ${OSL_OSLCOMP_LIBRARY})
if(APPLE)
  list(APPEND _osl_LIBRARIES -force_load ${OSL_OSLEXEC_LIBRARY})
else()
  list(APPEND _osl_LIBRARIES ${OSL_OSLEXEC_LIBRARY})
endif()
list(APPEND _osl_LIBRARIES ${OSL_OSLQUERY_LIBRARY})
if(OSL_OSLNOISE_LIBRARY)
  list(APPEND _osl_LIBRARIES ${OSL_OSLNOISE_LIBRARY})
endif()

find_program(OSL_COMPILER oslc
             HINTS ${_osl_SEARCH_DIRS}
             PATH_SUFFIXES bin)

get_filename_component(OSL_SHADER_HINT ${OSL_COMPILER} DIRECTORY)
get_filename_component(OSL_SHADER_HINT ${OSL_SHADER_DIR}/../ ABSOLUTE)

find_path(OSL_SHADER_DIR
  NAMES
    stdosl.h
  HINTS
    ${OSL_ROOT_DIR}
    ${OSL_SHADER_HINT}
    ${OSL_HOME_DIR}
    /usr/share/OSL/
    /usr/include/OSL/
  PATH_SUFFIXES
    share/OSL/shaders
    shaders
)

# handle the QUIETLY and REQUIRED arguments and set OSL_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OSL DEFAULT_MSG _osl_LIBRARIES OSL_INCLUDE_DIR OSL_COMPILER)

if(OSL_FOUND)
  set(OSL_LIBRARIES ${_osl_LIBRARIES})
  set(OSL_INCLUDE_DIRS ${OSL_INCLUDE_DIR})

  file(STRINGS "${OSL_INCLUDE_DIR}/OSL/oslversion.h" OSL_LIBRARY_VERSION_MAJOR
       REGEX "^[ \t]*#define[ \t]+OSL_LIBRARY_VERSION_MAJOR[ \t]+[0-9]+.*$")
  file(STRINGS "${OSL_INCLUDE_DIR}/OSL/oslversion.h" OSL_LIBRARY_VERSION_MINOR
       REGEX "^[ \t]*#define[ \t]+OSL_LIBRARY_VERSION_MINOR[ \t]+[0-9]+.*$")
  file(STRINGS "${OSL_INCLUDE_DIR}/OSL/oslversion.h" OSL_LIBRARY_VERSION_PATCH
       REGEX "^[ \t]*#define[ \t]+OSL_LIBRARY_VERSION_PATCH[ \t]+[0-9]+.*$")
  string(REGEX REPLACE ".*#define[ \t]+OSL_LIBRARY_VERSION_MAJOR[ \t]+([.0-9]+).*"
         "\\1" OSL_LIBRARY_VERSION_MAJOR ${OSL_LIBRARY_VERSION_MAJOR})
  string(REGEX REPLACE ".*#define[ \t]+OSL_LIBRARY_VERSION_MINOR[ \t]+([.0-9]+).*"
         "\\1" OSL_LIBRARY_VERSION_MINOR ${OSL_LIBRARY_VERSION_MINOR})
  string(REGEX REPLACE ".*#define[ \t]+OSL_LIBRARY_VERSION_PATCH[ \t]+([.0-9]+).*"
         "\\1" OSL_LIBRARY_VERSION_PATCH ${OSL_LIBRARY_VERSION_PATCH})
endif()

mark_as_advanced(
  OSL_INCLUDE_DIR
  OSL_SHADER_DIR
)

foreach(COMPONENT ${_osl_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  mark_as_advanced(OSL_${UPPERCOMPONENT}_LIBRARY)
endforeach()

unset(COMPONENT)
unset(UPPERCOMPONENT)

unset(_osl_FIND_COMPONENTS)
unset(_osl_LIBRARIES)
unset(_osl_SEARCH_DIRS)
