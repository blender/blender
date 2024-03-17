# SPDX-FileCopyrightText: 2011 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

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
#  PYTHON_VERSION_NO_DOTS
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

# If `PYTHON_ROOT_DIR` was defined in the environment, use it.
if(DEFINED PYTHON_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{PYTHON_ROOT_DIR})
  set(PYTHON_ROOT_DIR $ENV{PYTHON_ROOT_DIR})
else()
  set(PYTHON_ROOT_DIR "")
endif()

set(_PYTHON_VERSION_SUPPORTED 3.11)

set(PYTHON_VERSION ${_PYTHON_VERSION_SUPPORTED} CACHE STRING "Python Version (major and minor only)")
mark_as_advanced(PYTHON_VERSION)


if(APPLE)
  if(WITH_PYTHON_MODULE)
    set(PYTHON_LINKFLAGS "-undefined dynamic_lookup")
  else()
    set(PYTHON_LINKFLAGS)
  endif()
else()
  # See: http://docs.python.org/extending/embedding.html#linking-requirements
  set(PYTHON_LINKFLAGS "-Xlinker -export-dynamic" CACHE STRING "Linker flags for python")
  mark_as_advanced(PYTHON_LINKFLAGS)
endif()

# if the user passes these defines as args, we don't want to overwrite
set(_IS_INC_DEF OFF)
set(_IS_INC_CONF_DEF OFF)
set(_IS_LIB_DEF OFF)
set(_IS_LIB_PATH_DEF OFF)
if(DEFINED PYTHON_INCLUDE_DIR)
  set(_IS_INC_DEF ON)
endif()
if(DEFINED PYTHON_INCLUDE_CONFIG_DIR)
  set(_IS_INC_CONF_DEF ON)
endif()
if(DEFINED PYTHON_LIBRARY)
  set(_IS_LIB_DEF ON)
endif()
if(DEFINED PYTHON_LIBPATH)
  set(_IS_LIB_PATH_DEF ON)
endif()

string(REPLACE "." "" PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})

set(_PYTHON_ABI_FLAGS "")

set(_python_SEARCH_DIRS
  ${PYTHON_ROOT_DIR}
  "$ENV{HOME}/py${PYTHON_VERSION_NO_DOTS}"
  "/opt/lib/python-${PYTHON_VERSION}"
)

# only search for the dirs if we haven't already
if((NOT _IS_INC_DEF) OR (NOT _IS_INC_CONF_DEF) OR (NOT _IS_LIB_DEF) OR (NOT _IS_LIB_PATH_DEF))
  set(_PYTHON_ABI_FLAGS_TEST
    "u; "  # release
    "du;d" # debug
  )

  foreach(_CURRENT_ABI_FLAGS ${_PYTHON_ABI_FLAGS_TEST})
    # if(CMAKE_BUILD_TYPE STREQUAL Debug)
    #   set(_CURRENT_ABI_FLAGS "d${_CURRENT_ABI_FLAGS}")
    # endif()
    string(REPLACE " " "" _CURRENT_ABI_FLAGS ${_CURRENT_ABI_FLAGS})

    if(NOT DEFINED PYTHON_INCLUDE_DIR)
      find_path(PYTHON_INCLUDE_DIR
        NAMES
          Python.h
        HINTS
          ${_python_SEARCH_DIRS}
        PATH_SUFFIXES
          include/python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}
          include/${CMAKE_LIBRARY_ARCHITECTURE}/python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}
      )
    endif()

    if(NOT DEFINED PYTHON_INCLUDE_CONFIG_DIR)
      find_path(PYTHON_INCLUDE_CONFIG_DIR
        NAMES
          pyconfig.h
        HINTS
          ${_python_SEARCH_DIRS}
        PATH_SUFFIXES
          include/python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}
          include/${CMAKE_LIBRARY_ARCHITECTURE}/python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}
      )
      if((NOT PYTHON_INCLUDE_CONFIG_DIR) AND PYTHON_INCLUDE_DIR)
        # Fallback...
        unset(PYTHON_INCLUDE_CONFIG_DIR CACHE)
        set(PYTHON_INCLUDE_CONFIG_DIR ${PYTHON_INCLUDE_DIR} CACHE PATH "")
      endif()
    endif()

    if(NOT DEFINED PYTHON_LIBRARY)
      find_library(PYTHON_LIBRARY
        NAMES
          "python${PYTHON_VERSION}${_CURRENT_ABI_FLAGS}"
        HINTS
          ${_python_SEARCH_DIRS}
        PATH_SUFFIXES
          lib64 lib
      )
    endif()

    if(NOT DEFINED PYTHON_LIBPATH)
      find_path(PYTHON_LIBPATH
        NAMES
          "python${PYTHON_VERSION}/abc.py" # This is a bit hackish! :/
        HINTS
          ${_python_SEARCH_DIRS}
        PATH_SUFFIXES
          lib64 lib
      )
      if((NOT PYTHON_LIBPATH) AND PYTHON_LIBRARY)
        # Fallback...
        unset(PYTHON_LIBPATH CACHE)
        get_filename_component(PYTHON_LIBPATH ${PYTHON_LIBRARY} PATH)
      endif()
    endif()

    if(PYTHON_LIBRARY AND PYTHON_LIBPATH AND PYTHON_INCLUDE_DIR AND PYTHON_INCLUDE_CONFIG_DIR)
      set(_PYTHON_ABI_FLAGS "${_CURRENT_ABI_FLAGS}")
      break()
    else()
      # ensure we don't find values from 2 different ABI versions
      if(NOT _IS_INC_DEF)
        unset(PYTHON_INCLUDE_DIR CACHE)
      endif()
      if(NOT _IS_INC_CONF_DEF)
        unset(PYTHON_INCLUDE_CONFIG_DIR CACHE)
      endif()
      if(NOT _IS_LIB_DEF)
        unset(PYTHON_LIBRARY CACHE)
      endif()
      if(NOT _IS_LIB_PATH_DEF)
        unset(PYTHON_LIBPATH CACHE)
      endif()
    endif()
  endforeach()

  unset(_CURRENT_ABI_FLAGS)
  unset(_CURRENT_PATH)

  unset(_PYTHON_ABI_FLAGS_TEST)
endif()

unset(_IS_INC_DEF)
unset(_IS_INC_CONF_DEF)
unset(_IS_LIB_DEF)
unset(_IS_LIB_PATH_DEF)

# handle the QUIETLY and REQUIRED arguments and SET PYTHONLIBSUNIX_FOUND to TRUE IF
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)

# NOTE(@ideasman42): Instead of `DEFAULT_MSG` use a custom message because users
# may have newer versions Python and not be using pre-compiled libraries
# (on other UNIX systems or using an esoteric architecture).
# Some Python developers might want to use the newer features of Python too.
# While we could automatically detect and use newer versions but this would result in
# developers using a configuration which isn't officially supported without realizing it.
# So warn that the officially supported Python version is not found and let the developer
# explicitly set the newer version if they wish.
# From a maintenance perspective it's typically not a problem to support newer versions,
# doing so can help ease the process of upgrading too, nevertheless these versions don't
# have the same level of testing & support.
if(${_PYTHON_VERSION_SUPPORTED} STREQUAL ${PYTHON_VERSION})
  # Default version.
  set(_python_MISSING_MSG
    "\
'PYTHON_VERSION=${_PYTHON_VERSION_SUPPORTED}' not found! \
This is the only officially supported version. \
If you wish to use a newer Python version you may set 'PYTHON_VERSION' \
however we do not guarantee full compatibility in this case."
  )
else()
  # Default version overridden, use a different message.
  set(_python_MISSING_MSG
    "\
'PYTHON_VERSION=${PYTHON_VERSION}' not found! \
This is *not* the officially supported version. \
Either configure 'PYTHON_ROOT_DIR' to point to the Python installation \
or use the officially supported version ('${_PYTHON_VERSION_SUPPORTED}') \
which may have pre-compiled binaries for your platform."
  )
endif()

find_package_handle_standard_args(PythonLibsUnix
  "${_python_MISSING_MSG}"
  PYTHON_LIBRARY PYTHON_LIBPATH PYTHON_INCLUDE_DIR PYTHON_INCLUDE_CONFIG_DIR)
unset(_python_MISSING_MSG)

if(PYTHONLIBSUNIX_FOUND)
  # Assign cache items
  set(PYTHON_INCLUDE_DIRS ${PYTHON_INCLUDE_DIR} ${PYTHON_INCLUDE_CONFIG_DIR})
  if(NOT WITH_PYTHON_MODULE)
    set(PYTHON_LIBRARIES ${PYTHON_LIBRARY})
  endif()

  find_file(PYTHON_SITE_PACKAGES
    NAMES
      # debian specific
      dist-packages
      site-packages
    HINTS
      ${PYTHON_LIBPATH}/python${PYTHON_VERSION}
  )

  # we need this for installation
  # XXX No more valid with debian-like py3.5 packages...
  # get_filename_component(PYTHON_LIBPATH ${PYTHON_LIBRARY} PATH)

  # not required for build, just used when bundling Python.
  find_program(
    PYTHON_EXECUTABLE
    NAMES
      "python${PYTHON_VERSION}${_PYTHON_ABI_FLAGS}"
      "python${PYTHON_VERSION}"
      "python"
    HINTS
      ${_python_SEARCH_DIRS}
    PATH_SUFFIXES bin
  )
endif()

mark_as_advanced(
  PYTHON_INCLUDE_DIR
  PYTHON_INCLUDE_CONFIG_DIR
  PYTHON_LIBRARY
  PYTHON_LIBPATH
  PYTHON_SITE_PACKAGES
  PYTHON_EXECUTABLE
)

unset(_PYTHON_ABI_FLAGS)
unset(_PYTHON_VERSION_SUPPORTED)
unset(_python_SEARCH_DIRS)
