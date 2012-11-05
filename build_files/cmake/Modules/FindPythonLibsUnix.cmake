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

SET(PYTHON_VERSION 3.3 CACHE STRING "Python Version (major and minor only)")
MARK_AS_ADVANCED(PYTHON_VERSION)


# See: http://docs.python.org/extending/embedding.html#linking-requirements
#      for why this is needed
SET(PYTHON_LINKFLAGS "-Xlinker -export-dynamic" CACHE STRING "Linker flags for python")
MARK_AS_ADVANCED(PYTHON_LINKFLAGS)


# if the user passes these defines as args, we dont want to overwrite
SET(_IS_INC_DEF OFF)
SET(_IS_LIB_DEF OFF)
IF(DEFINED PYTHON_INCLUDE_DIR)
  SET(_IS_INC_DEF ON)
ENDIF()
IF(DEFINED PYTHON_LIBRARY)
  SET(_IS_LIB_DEF ON)
ENDIF()


# only search for the dirs if we havn't already
IF((NOT _IS_INC_DEF) OR (NOT _IS_LIB_DEF))

  SET(_python_ABI_FLAGS
    "m;mu;u; "    # release
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

    IF(NOT DEFINED PYTHON_INCLUDE_DIR)
      FIND_PATH(PYTHON_INCLUDE_DIR
        NAMES
          Python.h
        HINTS
          ${_python_SEARCH_DIRS}
        PATH_SUFFIXES
          include/python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}
      )
    ENDIF()

    IF(NOT DEFINED PYTHON_LIBRARY)
      FIND_LIBRARY(PYTHON_LIBRARY
        NAMES
          "python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}"
        HINTS
          ${_python_SEARCH_DIRS}
        PATH_SUFFIXES
          lib64 lib
      )
    ENDIF()

    IF(PYTHON_LIBRARY AND PYTHON_INCLUDE_DIR)
      break()
    ELSE()
      # ensure we dont find values from 2 different ABI versions
      IF(NOT _IS_INC_DEF)
        UNSET(PYTHON_INCLUDE_DIR CACHE)
      ENDIF()
      IF(NOT _IS_LIB_DEF)
        UNSET(PYTHON_LIBRARY CACHE)
      ENDIF()
    ENDIF()
  ENDFOREACH()

  UNSET(_CURRENT_ABI_FLAGS)
  UNSET(_CURRENT_PATH)

  UNSET(_python_ABI_FLAGS)
  UNSET(_python_SEARCH_DIRS)
ENDIF()

UNSET(_IS_INC_DEF)
UNSET(_IS_LIB_DEF)

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
ENDIF()

MARK_AS_ADVANCED(
  PYTHON_INCLUDE_DIR
  PYTHON_LIBRARY
)
