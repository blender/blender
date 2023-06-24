# SPDX-FileCopyrightText: 2016 Blender Foundation
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find CPPUNIT library
# Find the native CPPUNIT includes and library
# This module defines
#  CPPUNIT_INCLUDE_DIRS, where to find cppunit.h, Set when
#                    CPPUNIT is found.
#  CPPUNIT_LIBRARIES, libraries to link against to use CPPUNIT.
#  CPPUNIT_ROOT_DIR, The base directory to search for CPPUNIT.
#                This can also be an environment variable.
#  CPPUNIT_FOUND, If false, do not try to use CPPUNIT.
#
# also defined, but not for general use are
#  CPPUNIT_LIBRARY, where to find the CPPUNIT library.

# If CPPUNIT_ROOT_DIR was defined in the environment, use it.
IF(NOT CPPUNIT_ROOT_DIR AND NOT $ENV{CPPUNIT_ROOT_DIR} STREQUAL "")
  SET(CPPUNIT_ROOT_DIR $ENV{CPPUNIT_ROOT_DIR})
ENDIF()

SET(_cppunit_SEARCH_DIRS
  ${CPPUNIT_ROOT_DIR}
  /opt/lib/cppunit
)

FIND_PATH(CPPUNIT_INCLUDE_DIR
  NAMES
    cppunit/Test.h
  HINTS
    ${_cppunit_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(CPPUNIT_LIBRARY
  NAMES
    cppunit
  HINTS
    ${_cppunit_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
)

# handle the QUIETLY and REQUIRED arguments and set CPPUNIT_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(CPPUNIT DEFAULT_MSG
    CPPUNIT_LIBRARY CPPUNIT_INCLUDE_DIR)

IF(CPPUNIT_FOUND)
  SET(CPPUNIT_LIBRARIES ${CPPUNIT_LIBRARY})
  SET(CPPUNIT_INCLUDE_DIRS ${CPPUNIT_INCLUDE_DIR})
ELSE()
  SET(CPPUNIT_FOUND FALSE)
ENDIF()

MARK_AS_ADVANCED(
  CPPUNIT_INCLUDE_DIR
  CPPUNIT_LIBRARY
)
