# - Find OpenCOLLADA library
# Find the native OpenCOLLADA includes and library
# This module defines
#  OPENCOLLADA_INCLUDE_DIRS, where to find COLLADABaseUtils/ and 
#                 COLLADAFramework/, Set when OPENCOLLADA_INCLUDE_DIR is found.
#  OPENCOLLADA_LIBRARIES, libraries to link against to use OpenCOLLADA.
#  OPENCOLLADA_ROOT_DIR, The base directory to search for OpenCOLLADA.
#                    This can also be an environment variable.
#  OPENCOLLADA_FOUND, If false, do not try to use OpenCOLLADA.
#
# also defined, but not for general use are
#  OPENCOLLADA_LIBRARY, where to find the OpenCOLLADA library.

#=============================================================================
# Copyright 2011 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# note about include paths, there are 2 ways includes are set
#
# Where '/usr/include/opencollada' is the root dir:
#   /usr/include/opencollada/COLLADABaseUtils/COLLADABUPlatform.h
#
# Where '/opt/opencollada' is the base dir:
# /opt/opencollada/COLLADABaseUtils/include/COLLADABUPlatform.h

# If OPENCOLLADA_ROOT_DIR was defined in the environment, use it.
IF(NOT OPENCOLLADA_ROOT_DIR AND NOT $ENV{OPENCOLLADA_ROOT_DIR} STREQUAL "")
  SET(OPENCOLLADA_ROOT_DIR $ENV{OPENCOLLADA_ROOT_DIR})
ENDIF()

SET(_opencollada_FIND_INCLUDES
  COLLADAStreamWriter
  COLLADABaseUtils
  COLLADAFramework
  COLLADASaxFrameworkLoader
  GeneratedSaxParser
)

SET(_opencollada_FIND_COMPONENTS
  OpenCOLLADAStreamWriter
  OpenCOLLADASaxFrameworkLoader
  OpenCOLLADAFramework
  OpenCOLLADABaseUtils
  GeneratedSaxParser
  MathMLSolver
)

# Fedora openCOLLADA package links these statically
# note that order is important here ot it wont link
SET(_opencollada_FIND_STATIC_COMPONENTS
  buffer
  ftoa
  UTF
)

SET(_opencollada_SEARCH_DIRS
  ${OPENCOLLADA_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt/lib/opencollada
)

SET(_opencollada_INCLUDES)
FOREACH(COMPONENT ${_opencollada_FIND_INCLUDES})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  # need to use this even thouh we are looking for a dir
  FIND_FILE(OPENCOLLADA_${UPPERCOMPONENT}_INCLUDE_DIR
    NAMES
      ${COMPONENT}/include
      ${COMPONENT}
      # Ubuntu ppa needs this.
      # Alternative would be to suffix all members of search path
      # but this is less trouble, just looks strange.
      include/opencollada/${COMPONENT}
      include/${COMPONENT}/include
    HINTS
      ${_opencollada_SEARCH_DIRS}
    )
  MARK_AS_ADVANCED(OPENCOLLADA_${UPPERCOMPONENT}_INCLUDE_DIR)
  LIST(APPEND _opencollada_INCLUDES "${OPENCOLLADA_${UPPERCOMPONENT}_INCLUDE_DIR}")
ENDFOREACH()


SET(_opencollada_LIBRARIES)
FOREACH(COMPONENT ${_opencollada_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_opencollada_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
      # Ubuntu ppa needs this.
      lib64/opencollada lib/opencollada
    )
  MARK_AS_ADVANCED(OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY)
  LIST(APPEND _opencollada_LIBRARIES "${OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY}")
ENDFOREACH()

FOREACH(COMPONENT ${_opencollada_FIND_STATIC_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_opencollada_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
      # Ubuntu ppa needs this.
      lib64/opencollada lib/opencollada
    )
  MARK_AS_ADVANCED(OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY)
  IF(OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY)
    LIST(APPEND _opencollada_LIBRARIES "${OPENCOLLADA_${UPPERCOMPONENT}_LIBRARY}")
  ENDIF()
ENDFOREACH()


# handle the QUIETLY and REQUIRED arguments and set OPENCOLLADA_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenCOLLADA  DEFAULT_MSG
    _opencollada_LIBRARIES _opencollada_INCLUDES)


IF(OPENCOLLADA_FOUND)
  SET(OPENCOLLADA_LIBRARIES ${_opencollada_LIBRARIES})
  SET(OPENCOLLADA_INCLUDE_DIRS ${_opencollada_INCLUDES})
ENDIF(OPENCOLLADA_FOUND)
