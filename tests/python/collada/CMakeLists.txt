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
# ***** END GPL LICENSE BLOCK *****

# --env-system-scripts allows to run without the install target.

# Use '--write-blend=/tmp/test.blend' to view output

# Some tests are interesting but take too long to run
# and don't give deterministic results
set(USE_EXPERIMENTAL_TESTS FALSE)

set(TEST_SRC_DIR ${CMAKE_SOURCE_DIR}/../lib/tests)
set(TEST_OUT_DIR ${CMAKE_BINARY_DIR}/tests)

# ugh, any better way to do this on testing only?
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_OUT_DIR})

#~	if(NOT IS_DIRECTORY ${TEST_SRC_DIR})
#~		message(FATAL_ERROR "CMake test directory not found!")
#~	endif()

# all calls to blender use this
if(APPLE)
	if(${CMAKE_GENERATOR} MATCHES "Xcode")
		set(TEST_BLENDER_EXE_PARAMS --background -noaudio --factory-startup)
	else()
		set(TEST_BLENDER_EXE_PARAMS --background -noaudio --factory-startup --env-system-scripts ${CMAKE_SOURCE_DIR}/release/scripts)
	endif()
else()
	set(TEST_BLENDER_EXE_PARAMS --background -noaudio --factory-startup --env-system-scripts ${CMAKE_SOURCE_DIR}/release/scripts)
endif()

# for testing with valgrind prefix: valgrind --track-origins=yes --error-limit=no
# set(TEST_BLENDER_EXE_BARE ${TEST_BLENDER_EXE})
# set(TEST_BLENDER_EXE ${TEST_BLENDER_EXE} ${TEST_BLENDER_EXE_PARAMS} )

# ------------------------------------------------------------------------------
# GENERAL PYTHON CORRECTNESS TESTS
macro (COLLADA_TEST module test_name blend_file)
	add_test(
		NAME collada_${module}_${test_name}
		COMMAND "$<TARGET_FILE:blender>" ${TEST_BLENDER_EXE_PARAMS} ${TEST_SRC_DIR}/collada/${module}/${blend_file}
		--python ${CMAKE_CURRENT_LIST_DIR}/${module}/test_${module}_${test_name}.py --
		--testdir ${TEST_SRC_DIR}/collada/${module}
	)
endmacro()

# Tests are disabled because they only work on Windows
# Tests will be redone completely to work reliable
#COLLADA_TEST(mesh simple mesh_simple.blend)
#COLLADA_TEST(animation simple suzannes_parent_inverse.blend)
