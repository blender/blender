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
# Contributor(s): Jacques Beaurain.
#
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
		set(TEST_BLENDER_EXE ${EXECUTABLE_OUTPUT_PATH}/Debug/blender.app/Contents/MacOS/blender)
	else()
		set(TEST_BLENDER_EXE ${EXECUTABLE_OUTPUT_PATH}/blender.app/Contents/MacOS/blender)
	endif()
else()
	set(TEST_BLENDER_EXE ${EXECUTABLE_OUTPUT_PATH}/blender)
endif()

# for testing with valgrind prefix: valgrind --track-origins=yes --error-limit=no 
set(TEST_BLENDER_EXE_BARE ${TEST_BLENDER_EXE})
set(TEST_BLENDER_EXE ${TEST_BLENDER_EXE} --background -noaudio --factory-startup --env-system-scripts ${CMAKE_SOURCE_DIR}/release/scripts)


# ------------------------------------------------------------------------------

add_test(render_layer_scene_write_read ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/test_scene_write_read.py --
	--testdir="${TEST_SRC_DIR}/layers"
)

add_test(render_layer_scene_copy ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/test_scene_copy.py --
	--testdir="${TEST_SRC_DIR}/layers"
)

add_test(render_layer_syncing ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/test_syncing.py --
	--testdir="${TEST_SRC_DIR}/layers"
)

add_test(render_layer_layer_linking ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/test_layer_linking.py --
	--testdir="${TEST_SRC_DIR}/layers"
)

add_test(render_layer_active_collection ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/test_active_collection.py --
	--testdir="${TEST_SRC_DIR}/layers"
)

add_test(render_layer_object_delete ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/test_object_delete.py --
	--testdir="${TEST_SRC_DIR}/layers"
)

add_test(render_layer_link ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/test_link.py --
	--testdir="${TEST_SRC_DIR}/layers"
)

add_test(render_layer_operator_context ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/test_operator_context.py --
	--testdir="${TEST_SRC_DIR}/layers"
)

add_test(render_layer_object_add ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/test_object_add.py --
	--testdir="${TEST_SRC_DIR}/layers"
)

add_test(render_layer_object_copy ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/test_object_copy.py --
	--testdir="${TEST_SRC_DIR}/layers"
)

