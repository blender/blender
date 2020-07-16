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
# The Original Code is Copyright (C) 2020, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

# Build the test runner. This runner takes care of running all GTests, i.e.
# the code that was built using the blender_add_test_lib() CMake macro (see
# macros.cmake).
set(SRC
  blender_test.cc
)

if(WITH_BUILDINFO)
  list(APPEND SRC
    "$<TARGET_OBJECTS:buildinfoobj>"
  )
endif()


# Test libraries need to be linked "whole archive", because they're not
# directly referenced from other code.
get_property(_test_libs GLOBAL PROPERTY BLENDER_TEST_LIBS)
if(WIN32)
  list(APPEND TEST_LIBS ${_test_libs})
elseif(APPLE)
  list(APPEND TEST_LIBS "-Wl,-force_load" ${_test_libs})
elseif(UNIX)
  list(APPEND TEST_LIBS "-Wl,--whole-archive" ${_test_libs} "-Wl,--no-whole-archive")
else()
  message(FATAL_ERROR "Unknown how to link whole-archive with your compiler ${CMAKE_CXX_COMPILER_ID}")
endif()
unset(_test_libs)

# This builds `bin/tests/blender_test`, but does not add it as a single test.
setup_libdirs()
BLENDER_SRC_GTEST_EX(
  NAME blender
  SRC "${SRC}"
  EXTRA_LIBS "${TEST_LIBS}"
  SKIP_ADD_TEST
)
setup_liblinks(blender_test)

# This runs the blender_test executable with `--gtest_list_tests`, then
# exposes those tests individually to the ctest runner.
# See https://cmake.org/cmake/help/v3.18/module/GoogleTest.html
include(GoogleTest)

set(_GOOGLETEST_DISCOVER_TESTS_SCRIPT
  ${CMAKE_CURRENT_LIST_DIR}/BlenderAddTests.cmake
)

gtest_discover_tests(blender_test
  # So that unit tests know where to find files:
  EXTRA_ARGS
    --test-assets-dir "${CMAKE_SOURCE_DIR}/../lib/tests"
    --test-release-dir "$<TARGET_FILE_DIR:blender>/${BLENDER_VERSION}"
)
