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
IF(DEFINED OSL_ROOT_DIR)
  # Pass.
ELSEIF(DEFINED ENV{OSL_ROOT_DIR})
  SET(OSL_ROOT_DIR $ENV{OSL_ROOT_DIR})
ELSE()
  SET(OSL_ROOT_DIR "")
ENDIF()

SET(_osl_FIND_COMPONENTS
  oslnoise
  oslcomp
  oslexec
  oslquery
)

SET(_osl_SEARCH_DIRS
  ${OSL_ROOT_DIR}
  /opt/lib/osl
)

FIND_PATH(OSL_INCLUDE_DIR
  NAMES
    OSL/oslversion.h
  HINTS
    ${_osl_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FOREACH(COMPONENT ${_osl_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(OSL_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_osl_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
ENDFOREACH()

# Note linking order matters, and oslnoise existence depends on version.
SET(_osl_LIBRARIES ${OSL_OSLCOMP_LIBRARY})
IF(APPLE)
  list(APPEND _osl_LIBRARIES -force_load ${OSL_OSLEXEC_LIBRARY})
ELSE()
  list(APPEND _osl_LIBRARIES ${OSL_OSLEXEC_LIBRARY})
ENDIF()
list(APPEND _osl_LIBRARIES ${OSL_OSLQUERY_LIBRARY})
IF(OSL_OSLNOISE_LIBRARY)
  list(APPEND _osl_LIBRARIES ${OSL_OSLNOISE_LIBRARY})
ENDIF()

FIND_PROGRAM(OSL_COMPILER oslc
             HINTS ${_osl_SEARCH_DIRS}
             PATH_SUFFIXES bin)

get_filename_component(OSL_SHADER_HINT ${OSL_COMPILER} DIRECTORY)
get_filename_component(OSL_SHADER_HINT ${OSL_SHADER_DIR}/../ ABSOLUTE)

FIND_PATH(OSL_SHADER_DIR
  NAMES
    stdosl.h
  HINTS
    ${OSL_ROOT_DIR}
    ${OSL_SHADER_HINT}
    $ENV{OSLHOME}
    /usr/share/OSL/
    /usr/include/OSL/
  PATH_SUFFIXES
    share/OSL/shaders
    shaders
)

# handle the QUIETLY and REQUIRED arguments and set OSL_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OSL DEFAULT_MSG _osl_LIBRARIES OSL_INCLUDE_DIR OSL_COMPILER)

IF(OSL_FOUND)
  SET(OSL_LIBRARIES ${_osl_LIBRARIES})
  SET(OSL_INCLUDE_DIRS ${OSL_INCLUDE_DIR})

  FILE(STRINGS "${OSL_INCLUDE_DIR}/OSL/oslversion.h" OSL_LIBRARY_VERSION_MAJOR
       REGEX "^[ \t]*#define[ \t]+OSL_LIBRARY_VERSION_MAJOR[ \t]+[0-9]+.*$")
  FILE(STRINGS "${OSL_INCLUDE_DIR}/OSL/oslversion.h" OSL_LIBRARY_VERSION_MINOR
       REGEX "^[ \t]*#define[ \t]+OSL_LIBRARY_VERSION_MINOR[ \t]+[0-9]+.*$")
  FILE(STRINGS "${OSL_INCLUDE_DIR}/OSL/oslversion.h" OSL_LIBRARY_VERSION_PATCH
       REGEX "^[ \t]*#define[ \t]+OSL_LIBRARY_VERSION_PATCH[ \t]+[0-9]+.*$")
  STRING(REGEX REPLACE ".*#define[ \t]+OSL_LIBRARY_VERSION_MAJOR[ \t]+([.0-9]+).*"
         "\\1" OSL_LIBRARY_VERSION_MAJOR ${OSL_LIBRARY_VERSION_MAJOR})
  STRING(REGEX REPLACE ".*#define[ \t]+OSL_LIBRARY_VERSION_MINOR[ \t]+([.0-9]+).*"
         "\\1" OSL_LIBRARY_VERSION_MINOR ${OSL_LIBRARY_VERSION_MINOR})
  STRING(REGEX REPLACE ".*#define[ \t]+OSL_LIBRARY_VERSION_PATCH[ \t]+([.0-9]+).*"
         "\\1" OSL_LIBRARY_VERSION_PATCH ${OSL_LIBRARY_VERSION_PATCH})
ENDIF()

MARK_AS_ADVANCED(
  OSL_INCLUDE_DIR
  OSL_SHADER_DIR
)
FOREACH(COMPONENT ${_osl_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  MARK_AS_ADVANCED(OSL_${UPPERCOMPONENT}_LIBRARY)
ENDFOREACH()
