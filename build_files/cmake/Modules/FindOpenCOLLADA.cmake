# SPDX-FileCopyrightText: 2011 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find OpenCOLLADA library
# Find the native OpenCOLLADA includes and library
# This module defines
#  OPENCOLLADA_INCLUDE_DIRS, where to find COLLADABaseUtils/ and
#                 COLLADAFramework/, Set when OPENCOLLADA_INCLUDE_DIR is found.
#  OPENCOLLADA_LIBRARIES, libraries to link against to use OpenCOLLADA.
#  OPENCOLLADA_ROOT_DIR, The base directory to search for OpenCOLLADA.
#                    This can also be an environment variable.
#  OPENCOLLADA_FOUND, If false, do not try to use OpenCOLLADA.

# note about include paths, there are 2 ways includes are set
#
# Where '/usr/include/opencollada' is the root dir:
#   /usr/include/opencollada/COLLADABaseUtils/COLLADABUPlatform.h
#
# Where '/opt/opencollada' is the base dir:
# /opt/opencollada/COLLADABaseUtils/include/COLLADABUPlatform.h

# If `OPENCOLLADA_ROOT_DIR` was defined in the environment, use it.
if(DEFINED OPENCOLLADA_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{OPENCOLLADA_ROOT_DIR})
  set(OPENCOLLADA_ROOT_DIR $ENV{OPENCOLLADA_ROOT_DIR})
else()
  set(OPENCOLLADA_ROOT_DIR "")
endif()

set(_opencollada_FIND_INCLUDES
  COLLADAStreamWriter
  COLLADABaseUtils
  COLLADAFramework
  COLLADASaxFrameworkLoader
  GeneratedSaxParser
)

set(_opencollada_FIND_COMPONENTS
  OpenCOLLADAStreamWriter
  OpenCOLLADASaxFrameworkLoader
  OpenCOLLADAFramework
  OpenCOLLADABaseUtils
  GeneratedSaxParser
  MathMLSolver
)

# Fedora openCOLLADA package links these statically
# note that order is important here or it won't link
set(_opencollada_FIND_STATIC_COMPONENTS
  buffer
  ftoa
  UTF
)

set(_opencollada_SEARCH_DIRS
  ${OPENCOLLADA_ROOT_DIR}
  /opt/lib/opencollada
)

set(_opencollada_INCLUDES)
foreach(COMPONENT ${_opencollada_FIND_INCLUDES})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  # need to use this even thouh we are looking for a dir
  find_file(OPENCOLLADA_${UPPERCOMPONENT}_INCLUDE_DIR
    NAMES
      ${COMPONENT}/include
      ${COMPONENT}
      # Ubuntu ppa needs this.
      # Alternative would be to suffix all members of search path
      # but this is less trouble, just looks strange.
      include/opencollada/${COMPONENT}
      include/${COMPONENT}/include
      include/${COMPONENT}
    HINTS
      ${_opencollada_SEARCH_DIRS}
    )
  mark_as_advanced(OPENCOLLADA_${UPPERCOMPONENT}_INCLUDE_DIR)
  list(APPEND _opencollada_INCLUDES "${OPENCOLLADA_${UPPERCOMPONENT}_INCLUDE_DIR}")
endforeach()


set(_opencollada_LIBRARIES)
foreach(COMPONENT ${_opencollada_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  find_library(OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_opencollada_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
      # Ubuntu ppa needs this.
      lib64/opencollada lib/opencollada
    )
  mark_as_advanced(OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY)
  list(APPEND _opencollada_LIBRARIES "${OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY}")
endforeach()

foreach(COMPONENT ${_opencollada_FIND_STATIC_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  find_library(OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_opencollada_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
      # Ubuntu ppa needs this.
      lib64/opencollada lib/opencollada
    )
  mark_as_advanced(OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY)
  if(OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY)
    list(APPEND _opencollada_LIBRARIES "${OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY}")
  endif()
endforeach()


# handle the QUIETLY and REQUIRED arguments and set OPENCOLLADA_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCOLLADA  DEFAULT_MSG
    _opencollada_LIBRARIES _opencollada_INCLUDES)


if(OPENCOLLADA_FOUND)
  set(OPENCOLLADA_LIBRARIES ${_opencollada_LIBRARIES})
  set(OPENCOLLADA_INCLUDE_DIRS ${_opencollada_INCLUDES})
endif()

unset(COMPONENT)
unset(UPPERCOMPONENT)
unset(_opencollada_FIND_COMPONENTS)
unset(_opencollada_FIND_INCLUDES)
unset(_opencollada_FIND_STATIC_COMPONENTS)
unset(_opencollada_INCLUDES)
unset(_opencollada_LIBRARIES)
unset(_opencollada_SEARCH_DIRS)
