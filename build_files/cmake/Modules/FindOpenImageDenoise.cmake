# SPDX-FileCopyrightText: 2019 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find OpenImageDenoise library
# Find the native OpenImageDenoise includes and library
# This module defines
#  OPENIMAGEDENOISE_INCLUDE_DIRS, where to find oidn.h, Set when
#                    OPENIMAGEDENOISE is found.
#  OPENIMAGEDENOISE_LIBRARIES, libraries to link against to use OpenImageDenoise.
#  OPENIMAGEDENOISE_ROOT_DIR, The base directory to search for OpenImageDenoise.
#                This can also be an environment variable.
#  OPENIMAGEDENOISE_FOUND, If false, do not try to use OpenImageDenoise.
#
# also defined, but not for general use are
#  OPENIMAGEDENOISE_LIBRARY, where to find the OpenImageDenoise library.

# If `OPENIMAGEDENOISE_ROOT_DIR` was defined in the environment, use it.
if(DEFINED OPENIMAGEDENOISE_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{OPENIMAGEDENOISE_ROOT_DIR})
  set(OPENIMAGEDENOISE_ROOT_DIR $ENV{OPENIMAGEDENOISE_ROOT_DIR})
else()
  set(OPENIMAGEDENOISE_ROOT_DIR "")
endif()

set(_openimagedenoise_SEARCH_DIRS
  ${OPENIMAGEDENOISE_ROOT_DIR}
  /opt/lib/openimagedenoise
)

find_path(OPENIMAGEDENOISE_INCLUDE_DIR
  NAMES
    OpenImageDenoise/oidn.h
  HINTS
    ${_openimagedenoise_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

set(_openimagedenoise_FIND_COMPONENTS
  OpenImageDenoise
)

# These are needed when building statically
set(_openimagedenoise_FIND_STATIC_COMPONENTS
  common

  # These additional library names change between versions, we list all of them
  # so builds work with multiple versions. Missing libraries are skipped.
  dnnl_cpu
  dnnl_common
  dnnl_cpu # Second time because of circular dependency
  mkldnn
  dnnl
)

set(_openimagedenoise_LIBRARIES)
foreach(COMPONENT ${_openimagedenoise_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  find_library(OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_openimagedenoise_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  list(APPEND _openimagedenoise_LIBRARIES "${OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY}")
endforeach()

foreach(COMPONENT ${_openimagedenoise_FIND_STATIC_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  find_library(OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_openimagedenoise_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  mark_as_advanced(OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY)
  if(OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY)
    list(APPEND _openimagedenoise_LIBRARIES "${OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY}")
  endif()
endforeach()

find_library(OPENIMAGEDENOISE_LIBRARY
  NAMES
    OpenImageDenoise
  HINTS
    ${_openimagedenoise_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set OPENIMAGEDENOISE_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenImageDenoise DEFAULT_MSG
    OPENIMAGEDENOISE_LIBRARY OPENIMAGEDENOISE_INCLUDE_DIR)

if(OPENIMAGEDENOISE_FOUND)
  set(OPENIMAGEDENOISE_LIBRARIES ${_openimagedenoise_LIBRARIES})
  set(OPENIMAGEDENOISE_INCLUDE_DIRS ${OPENIMAGEDENOISE_INCLUDE_DIR})
else()
  set(OPENIMAGEDENOISE_FOUND FALSE)
endif()

mark_as_advanced(
  OPENIMAGEDENOISE_INCLUDE_DIR
  OPENIMAGEDENOISE_LIBRARY
)

foreach(COMPONENT ${_openimagedenoise_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  mark_as_advanced(OPENIMAGEDENOISE_${UPPERCOMPONENT}_LIBRARY)
endforeach()

unset(_openimagedenoise_SEARCH_DIRS)
unset(_openimagedenoise_FIND_COMPONENTS)
unset(_openimagedenoise_LIBRARIES)
