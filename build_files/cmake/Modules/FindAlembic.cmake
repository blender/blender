# SPDX-FileCopyrightText: 2016 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Alembic library
# Find the native Alembic includes and libraries
# This module defines
#  ALEMBIC_INCLUDE_DIRS, where to find Alembic headers, Set when
#                        ALEMBIC_INCLUDE_DIR is found.
#  ALEMBIC_LIBRARIES, libraries to link against to use Alembic.
#  ALEMBIC_ROOT_DIR, The base directory to search for Alembic.
#                    This can also be an environment variable.
#  ALEMBIC_FOUND, If false, do not try to use Alembic.
#

# If `ALEMBIC_ROOT_DIR` was defined in the environment, use it.
if(DEFINED ALEMBIC_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{ALEMBIC_ROOT_DIR})
  set(ALEMBIC_ROOT_DIR $ENV{ALEMBIC_ROOT_DIR})
else()
  set(ALEMBIC_ROOT_DIR "")
endif()

set(_alembic_SEARCH_DIRS
  ${ALEMBIC_ROOT_DIR}
  /opt/lib/alembic
)

find_path(ALEMBIC_INCLUDE_DIR
  NAMES
    Alembic/Abc/All.h
  HINTS
    ${_alembic_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

find_library(ALEMBIC_LIBRARY
  NAMES
    Alembic
  HINTS
    ${_alembic_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib lib/static
)

# handle the QUIETLY and REQUIRED arguments and set ALEMBIC_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Alembic DEFAULT_MSG ALEMBIC_LIBRARY ALEMBIC_INCLUDE_DIR)

if(ALEMBIC_FOUND)
  set(ALEMBIC_LIBRARIES ${ALEMBIC_LIBRARY})
  set(ALEMBIC_INCLUDE_DIRS ${ALEMBIC_INCLUDE_DIR})
endif()

mark_as_advanced(
  ALEMBIC_INCLUDE_DIR
  ALEMBIC_LIBRARY
)

unset(_alembic_SEARCH_DIRS)
