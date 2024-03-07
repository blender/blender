# SPDX-FileCopyrightText: 2014 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find PugiXML library
# Find the native PugiXML includes and library
# This module defines
#  PUGIXML_INCLUDE_DIRS, where to find pugixml.hpp, Set when
#                        PugiXML is found.
#  PUGIXML_LIBRARIES, libraries to link against to use PugiiXML.
#  PUGIXML_ROOT_DIR, The base directory to search for PugiXML.
#                    This can also be an environment variable.
#  PUGIXML_FOUND, If false, do not try to use PugiXML.
#
# also defined, but not for general use are
#  PUGIXML_LIBRARY, where to find the PugiXML library.

# If `PUGIXML_ROOT_DIR` was defined in the environment, use it.
if(DEFINED PUGIXML_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{PUGIXML_ROOT_DIR})
  set(PUGIXML_ROOT_DIR $ENV{PUGIXML_ROOT_DIR})
else()
  set(PUGIXML_ROOT_DIR "")
endif()

set(_pugixml_SEARCH_DIRS
  ${PUGIXML_ROOT_DIR}
  /opt/lib/oiio
)

find_path(PUGIXML_INCLUDE_DIR
  NAMES
    pugixml.hpp
  HINTS
    ${_pugixml_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(PUGIXML_LIBRARY
  NAMES
    pugixml
  HINTS
    ${_pugixml_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set PUGIXML_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PugiXML DEFAULT_MSG
    PUGIXML_LIBRARY PUGIXML_INCLUDE_DIR)

if(PUGIXML_FOUND)
  set(PUGIXML_LIBRARIES ${PUGIXML_LIBRARY})
  set(PUGIXML_INCLUDE_DIRS ${PUGIXML_INCLUDE_DIR})
else()
  set(PUGIXML_PUGIXML_FOUND FALSE)
endif()

mark_as_advanced(
  PUGIXML_INCLUDE_DIR
  PUGIXML_LIBRARY
)

unset(_pugixml_SEARCH_DIRS)
