# SPDX-FileCopyrightText: 2013 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find OpenSubdiv library
# Find the native OpenSubdiv includes and library
# This module defines
#  OPENSUBDIV_INCLUDE_DIRS, where to find OpenSubdiv headers, Set when
#                           OPENSUBDIV_INCLUDE_DIR is found.
#  OPENSUBDIV_LIBRARIES, libraries to link against to use OpenSubdiv.
#  OPENSUBDIV_ROOT_DIR, the base directory to search for OpenSubdiv.
#                        This can also be an environment variable.
#  OPENSUBDIV_FOUND, if false, do not try to use OpenSubdiv.

# If `OPENSUBDIV_ROOT_DIR` was defined in the environment, use it.
if(DEFINED OPENSUBDIV_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{OPENSUBDIV_ROOT_DIR})
  set(OPENSUBDIV_ROOT_DIR $ENV{OPENSUBDIV_ROOT_DIR})
else()
  set(OPENSUBDIV_ROOT_DIR "")
endif()

set(_opensubdiv_FIND_COMPONENTS
  osdGPU
  osdCPU
)

set(_opensubdiv_SEARCH_DIRS
  ${OPENSUBDIV_ROOT_DIR}
  /opt/lib/opensubdiv
  /opt/lib/osd # install_deps.sh
)

find_path(OPENSUBDIV_INCLUDE_DIR
  NAMES
    opensubdiv/osd/mesh.h
  HINTS
    ${_opensubdiv_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

set(_opensubdiv_LIBRARIES)
foreach(COMPONENT ${_opensubdiv_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  find_library(OPENSUBDIV_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_opensubdiv_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  list(APPEND _opensubdiv_LIBRARIES "${OPENSUBDIV_${UPPERCOMPONENT}_LIBRARY}")
endforeach()

macro(OPENSUBDIV_CHECK_CONTROLLER
      controller_include_file
      variable_name)
  if(EXISTS "${OPENSUBDIV_INCLUDE_DIR}/opensubdiv/osd/${controller_include_file}")
    set(${variable_name} TRUE)
  else()
    set(${variable_name} FALSE)
  endif()
endmacro()


# handle the QUIETLY and REQUIRED arguments and set OPENSUBDIV_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenSubdiv DEFAULT_MSG
    _opensubdiv_LIBRARIES OPENSUBDIV_INCLUDE_DIR)

if(OPENSUBDIV_FOUND)
  set(OPENSUBDIV_LIBRARIES ${_opensubdiv_LIBRARIES})
  set(OPENSUBDIV_INCLUDE_DIRS ${OPENSUBDIV_INCLUDE_DIR})
endif()

mark_as_advanced(
  OPENSUBDIV_INCLUDE_DIR
)
foreach(COMPONENT ${_opensubdiv_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  mark_as_advanced(OPENSUBDIV_${UPPERCOMPONENT}_LIBRARY)
endforeach()
