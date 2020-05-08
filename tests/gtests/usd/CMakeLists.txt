# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2019, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

# This suppresses the warning "This file includes at least one deprecated or antiquated
# header which may be removed without further notice at a future date", which is caused
# by the USD library including <ext/hash_set> on Linux. This has been reported at:
# https://github.com/PixarAnimationStudios/USD/issues/1057.
if(UNIX AND NOT APPLE)
  add_definitions(-D_GLIBCXX_PERMIT_BACKWARD_HASH)
endif()
if(WIN32)
  add_definitions(-DNOMINMAX)
endif()
add_definitions(-DPXR_STATIC)

set(INC
  .
  ..
  ../../../source/blender/blenlib
  ../../../source/blender/blenkernel
  ../../../source/blender/io/common
  ../../../source/blender/io/usd
  ../../../source/blender/makesdna
  ../../../source/blender/depsgraph
  ${USD_INCLUDE_DIRS}
  ${BOOST_INCLUDE_DIR}
  ${TBB_INCLUDE_DIR}
)

set(LIB
  bf_blenloader_test
  bf_blenloader

  # Should not be needed but gives windows linker errors if the ocio libs are linked before this:
  bf_intern_opencolorio
  bf_gpu

  bf_usd
  bf_io_common

  ${BOOST_LIBRARIES}
  ${TBB_LIBRARIES}
)

include_directories(${INC})

setup_libdirs()
get_property(BLENDER_SORTED_LIBS GLOBAL PROPERTY BLENDER_SORTED_LIBS_PROP)

set(SRC
  abstract_hierarchy_iterator_test.cc
  hierarchy_context_order_test.cc
)

# TODO(Sybren): re-enable this unit test.
# if(NOT APPLE)
#   # TODO(Sybren): This unit test has only been tested on Linux, and should possibly be
#   # restructured to support other platforms as well.
#   list(APPEND SRC usd_stage_creation_test.cc)
# endif()


if(WITH_BUILDINFO)
  list(APPEND SRC
    "$<TARGET_OBJECTS:buildinfoobj>"
  )
endif()

# get_cmake_property(_variableNames VARIABLES)
# list(SORT _variableNames)
# foreach(_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

# Works on Linux, not on Windows:
# set(_usd_DATAFILES_DIR "${CMAKE_INSTALL_PREFIX}/${BLENDER_VERSION}/datafiles/usd")
set(_usd_DATAFILES_DIR "$<TARGET_FILE_DIR:blender>/${BLENDER_VERSION}/datafiles/usd")

BLENDER_SRC_GTEST_EX(
  NAME usd
  SRC "${SRC}"
  EXTRA_LIBS "${LIB}"
  COMMAND_ARGS
    --test-assets-dir "${CMAKE_SOURCE_DIR}/../lib/tests"
)
# TODO(Sybren): add the below CLI argument to the test when the usd_stage_creation_test.cc
# test is reenabled.
# --test-usd-datafiles-dir "${_usd_DATAFILES_DIR}"
unset(_usd_DATAFILES_DIR)

setup_liblinks(usd_test)
