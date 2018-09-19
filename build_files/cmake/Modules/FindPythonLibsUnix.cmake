# - Find Python libraries
# Find the native Python includes and library
#
# Note:, This is not _yet_ intended to be a general python module for other
#  projects to use since its hard coded to fixed Python version
#  as Blender only supports a single Python version at the moment.
#
# Note:
#  this is for Blender/Unix Python only.
#
# This module defines
#  PYTHON_VERSION
#  PYTHON_INCLUDE_DIRS
#  PYTHON_INCLUDE_CONFIG_DIRS
#  PYTHON_LIBRARIES
#  PYTHON_LIBPATH, Used for installation
#  PYTHON_SITE_PACKAGES, Used for installation (as a Python module)
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

SET(PYTHON_VERSION 3.7 CACHE STRING "Python Version (major and minor only)")
MARK_AS_ADVANCED(PYTHON_VERSION)


# See: http://docs.python.org/extending/embedding.html#linking-requirements
#      for why this is needed
SET(PYTHON_LINKFLAGS "-Xlinker -export-dynamic" CACHE STRING "Linker flags for python")
MARK_AS_ADVANCED(PYTHON_LINKFLAGS)


# if the user passes these defines as args, we dont want to overwrite
SET(_IS_INC_DEF OFF)
SET(_IS_INC_CONF_DEF OFF)
SET(_IS_LIB_DEF OFF)
SET(_IS_LIB_PATH_DEF OFF)
IF(DEFINED PYTHON_INCLUDE_DIR)
  SET(_IS_INC_DEF ON)
ENDIF()
IF(DEFINED PYTHON_INCLUDE_CONFIG_DIR)
  SET(_IS_INC_CONF_DEF ON)
ENDIF()
IF(DEFINED PYTHON_LIBRARY)
  SET(_IS_LIB_DEF ON)
ENDIF()
IF(DEFINED PYTHON_LIBPATH)
  SET(_IS_LIB_PATH_DEF ON)
ENDIF()

STRING(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})

SET(_python_SEARCH_DIRS
  ${PYTHON_ROOT_DIR}
  "$ENV{HOME}/py${_PYTHON_VERSION_NO_DOTS}"
  "/opt/py${_PYTHON_VERSION_NO_DOTS}"
  "/opt/lib/python-${PYTHON_VERSION}"
)

# only search for the dirs if we haven't already
IF((NOT _IS_INC_DEF) OR (NOT _IS_INC_CONF_DEF) OR (NOT _IS_LIB_DEF) OR (NOT _IS_LIB_PATH_DEF))
  SET(_PYTHON_ABI_FLAGS_TEST
    "m;mu;u; "    # release
    "dm;dmu;du;d" # debug
  )

  FOREACH(_CURRENT_ABI_FLAGS ${_PYTHON_ABI_FLAGS_TEST})
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
          include/${CMAKE_LIBRARY_ARCHITECTURE}/python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}
      )
    ENDIF()

    IF(NOT DEFINED PYTHON_INCLUDE_CONFIG_DIR)
      FIND_PATH(PYTHON_INCLUDE_CONFIG_DIR
        NAMES
          pyconfig.h
        HINTS
          ${_python_SEARCH_DIRS}
        PATH_SUFFIXES
          include/python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}
          include/${CMAKE_LIBRARY_ARCHITECTURE}/python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}
      )
      IF((NOT PYTHON_INCLUDE_CONFIG_DIR) AND PYTHON_INCLUDE_DIR)
        # Fallback...
        UNSET(PYTHON_INCLUDE_CONFIG_DIR CACHE)
        SET(PYTHON_INCLUDE_CONFIG_DIR ${PYTHON_INCLUDE_DIR} CACHE PATH "")
      ENDIF()
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

    IF(NOT DEFINED PYTHON_LIBPATH)
      FIND_PATH(PYTHON_LIBPATH
        NAMES
          "python${PYTHON_VERSION}/abc.py" # This is a bit hackish! :/
        HINTS
          ${_python_SEARCH_DIRS}
        PATH_SUFFIXES
          lib64 lib
      )
      IF((NOT PYTHON_LIBPATH) AND PYTHON_LIBRARY)
        # Fallback...
        UNSET(PYTHON_LIBPATH CACHE)
        GET_FILENAME_COMPONENT(PYTHON_LIBPATH ${PYTHON_LIBRARY} PATH)
      ENDIF()
    ENDIF()

    IF(PYTHON_LIBRARY AND PYTHON_LIBPATH AND PYTHON_INCLUDE_DIR AND PYTHON_INCLUDE_CONFIG_DIR)
      SET(_PYTHON_ABI_FLAGS "${_CURRENT_ABI_FLAGS}")
      break()
    ELSE()
      # ensure we dont find values from 2 different ABI versions
      IF(NOT _IS_INC_DEF)
        UNSET(PYTHON_INCLUDE_DIR CACHE)
      ENDIF()
      IF(NOT _IS_INC_CONF_DEF)
        UNSET(PYTHON_INCLUDE_CONFIG_DIR CACHE)
      ENDIF()
      IF(NOT _IS_LIB_DEF)
        UNSET(PYTHON_LIBRARY CACHE)
      ENDIF()
      IF(NOT _IS_LIB_PATH_DEF)
        UNSET(PYTHON_LIBPATH CACHE)
      ENDIF()
    ENDIF()
  ENDFOREACH()

  UNSET(_CURRENT_ABI_FLAGS)
  UNSET(_CURRENT_PATH)

  UNSET(_PYTHON_ABI_FLAGS_TEST)
ENDIF()

UNSET(_IS_INC_DEF)
UNSET(_IS_INC_CONF_DEF)
UNSET(_IS_LIB_DEF)
UNSET(_IS_LIB_PATH_DEF)

# handle the QUIETLY and REQUIRED arguments and SET PYTHONLIBSUNIX_FOUND to TRUE IF
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PythonLibsUnix  DEFAULT_MSG
    PYTHON_LIBRARY PYTHON_LIBPATH PYTHON_INCLUDE_DIR PYTHON_INCLUDE_CONFIG_DIR)

IF(PYTHONLIBSUNIX_FOUND)
  # Assign cache items
  SET(PYTHON_INCLUDE_DIRS ${PYTHON_INCLUDE_DIR} ${PYTHON_INCLUDE_CONFIG_DIR})
  SET(PYTHON_LIBRARIES ${PYTHON_LIBRARY})

  FIND_FILE(PYTHON_SITE_PACKAGES
    NAMES
      # debian specific
      dist-packages
      site-packages
    HINTS
      ${PYTHON_LIBPATH}/python${PYTHON_VERSION}
  )

  # we need this for installation
  # XXX No more valid with debian-like py3.5 packages...
#  GET_FILENAME_COMPONENT(PYTHON_LIBPATH ${PYTHON_LIBRARY} PATH)

  # not required for build, just used when bundling Python.
  FIND_PROGRAM(
    PYTHON_EXECUTABLE
    NAMES
      "python${PYTHON_VERSION}${_PYTHON_ABI_FLAGS}"
      "python${PYTHON_VERSION}"
      "python"
    HINTS
      ${_python_SEARCH_DIRS}
    PATH_SUFFIXES bin
  )
ENDIF()

UNSET(_PYTHON_VERSION_NO_DOTS)
UNSET(_PYTHON_ABI_FLAGS)
UNSET(_python_SEARCH_DIRS)

MARK_AS_ADVANCED(
  PYTHON_INCLUDE_DIR
  PYTHON_INCLUDE_CONFIG_DIR
  PYTHON_LIBRARY
  PYTHON_LIBPATH
  PYTHON_SITE_PACKAGES
  PYTHON_EXECUTABLE
)
