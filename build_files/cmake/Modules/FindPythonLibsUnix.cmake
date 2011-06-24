# - Find Python libraries
# Find the native Python includes and library
#
# Note:, This is not _yet_ intended to be a general python module for other
#  projects to use since its hard coded to python 3.2 as blender only supports
#  a single python version.
#  This is for blender/unix python only.
#
# This module defines
#  PYTHON_VERSION
#  PYTHON_INCLUDE_DIRS
#  PYTHON_LIBRARIES
#  PYTHON_LIBPATH, Used for installation
#  PYTHON_LINKFLAGS
#  PYTHON_ROOT_DIR, The base directory to search for Python.
#                   This can also be an environment variable.
#
# also defined, but not for general use are
#  PYTHON_LIBRARY, where to find the python library.

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

# If PYTHON_ROOT_DIR was defined in the environment, use it.
IF(NOT PYTHON_ROOT_DIR AND NOT $ENV{PYTHON_ROOT_DIR} STREQUAL "")
  SET(PYTHON_ROOT_DIR $ENV{PYTHON_ROOT_DIR})
ENDIF()

IF(DEFINED PYTHON_VERSION)
  SET(PYTHON_VERSION "${PYTHON_VERSION}" CACHE STRING "")
ELSE()
  SET(PYTHON_VERSION 3.2 CACHE STRING "")
ENDIF()
MARK_AS_ADVANCED(PYTHON_VERSION)

SET(PYTHON_LINKFLAGS "-Xlinker -export-dynamic")
MARK_AS_ADVANCED(PYTHON_LINKFLAGS)

SET(_python_ABI_FLAGS
  "m;mu;u; "  # release
  "md;mud;ud;d" # debug
)

STRING(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})

SET(_python_SEARCH_DIRS
  ${PYTHON_ROOT_DIR}
  "$ENV{HOME}/py${_PYTHON_VERSION_NO_DOTS}"
  "/opt/py${_PYTHON_VERSION_NO_DOTS}"
)

FOREACH(_CURRENT_ABI_FLAGS ${_python_ABI_FLAGS})
  #IF(CMAKE_BUILD_TYPE STREQUAL Debug)
  #  SET(_CURRENT_ABI_FLAGS "d${_CURRENT_ABI_FLAGS}")
  #ENDIF()
  STRING(REPLACE " " "" _CURRENT_ABI_FLAGS ${_CURRENT_ABI_FLAGS})

  FIND_PATH(PYTHON_INCLUDE_DIR
    NAMES
      Python.h
    HINTS
      ${_python_SEARCH_DIRS}
    PATH_SUFFIXES
      include/python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}
  )

  FIND_LIBRARY(PYTHON_LIBRARY
    NAMES
      "python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}"
    HINTS
      ${_python_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
  )

  IF(PYTHON_LIBRARY AND PYTHON_INCLUDE_DIR)
    break()
  ELSE()
    # ensure we dont find values from 2 different ABI versions
    UNSET(PYTHON_INCLUDE_DIR CACHE)
    UNSET(PYTHON_LIBRARY CACHE)
  ENDIF()
ENDFOREACH()

UNSET(_CURRENT_ABI_FLAGS)
UNSET(_CURRENT_PATH)

UNSET(_python_ABI_FLAGS)
UNSET(_python_SEARCH_DIRS)

# handle the QUIETLY and REQUIRED arguments and SET PYTHONLIBSUNIX_FOUND to TRUE IF 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PythonLibsUnix  DEFAULT_MSG
    PYTHON_LIBRARY PYTHON_INCLUDE_DIR)


IF(PYTHONLIBSUNIX_FOUND)
  # Assign cache items
  SET(PYTHON_INCLUDE_DIRS ${PYTHON_INCLUDE_DIR})
  SET(PYTHON_LIBRARIES ${PYTHON_LIBRARY})

  # we need this for installation
  GET_FILENAME_COMPONENT(PYTHON_LIBPATH ${PYTHON_LIBRARY} PATH)

  # not used
  # SET(PYTHON_BINARY ${PYTHON_EXECUTABLE} CACHE STRING "")

  MARK_AS_ADVANCED(
    PYTHON_INCLUDE_DIR
    PYTHON_LIBRARY
  )
ENDIF()
