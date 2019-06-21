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

set(TEST_SRC_DIR ${CMAKE_SOURCE_DIR}/../lib/tests)
set(TEST_OUT_DIR ${CMAKE_BINARY_DIR}/tests)

# ugh, any better way to do this on testing only?
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_OUT_DIR})

#~  if(NOT IS_DIRECTORY ${TEST_SRC_DIR})
#~      message(FATAL_ERROR "CMake test directory not found!")
#~  endif()

# for testing with valgrind prefix: valgrind --track-origins=yes --error-limit=no
set(TEST_BLENDER_EXE $<TARGET_FILE:blender> --background -noaudio --factory-startup --env-system-scripts ${CMAKE_SOURCE_DIR}/release/scripts)


# ------------------------------------------------------------------------------

macro(VIEW_LAYER_TEST test_name)
  # Adds ${CMAKE_CURRENT_LIST_DIR} to sys.path so that the tests can import
  # things from view_layer_common.py
  add_test(view_layer_${test_name} ${TEST_BLENDER_EXE}
    --python-expr "import sys; sys.path.append('${CMAKE_CURRENT_LIST_DIR}')"
    --python-exit-code 1
    --python ${CMAKE_CURRENT_LIST_DIR}/test_${test_name}.py --
    --testdir="${TEST_SRC_DIR}/layers"
  )
endmacro()

VIEW_LAYER_TEST(active_collection)
VIEW_LAYER_TEST(background_set)
VIEW_LAYER_TEST(collection_new_sync)
VIEW_LAYER_TEST(collection_rename_a)
VIEW_LAYER_TEST(collection_rename_b)
#VIEW_LAYER_TEST(evaluation_render_settings_a)
#VIEW_LAYER_TEST(evaluation_render_settings_b)
#VIEW_LAYER_TEST(evaluation_render_settings_c)
#VIEW_LAYER_TEST(evaluation_render_settings_d)
#VIEW_LAYER_TEST(evaluation_render_settings_e)
#VIEW_LAYER_TEST(evaluation_render_settings_f)
#VIEW_LAYER_TEST(evaluation_render_settings_g)
#VIEW_LAYER_TEST(evaluation_render_settings_h)
#VIEW_LAYER_TEST(evaluation_render_settings_i)
VIEW_LAYER_TEST(evaluation_visibility_a)
VIEW_LAYER_TEST(evaluation_visibility_b)
VIEW_LAYER_TEST(evaluation_visibility_c)
VIEW_LAYER_TEST(evaluation_visibility_d)
VIEW_LAYER_TEST(evaluation_visibility_e)
VIEW_LAYER_TEST(evaluation_visibility_f)
VIEW_LAYER_TEST(evaluation_visibility_g)
VIEW_LAYER_TEST(evaluation_visibility_h)
VIEW_LAYER_TEST(evaluation_visibility_i)
VIEW_LAYER_TEST(evaluation_visibility_j)
VIEW_LAYER_TEST(evaluation_selectability_a)
VIEW_LAYER_TEST(evaluation_selectability_b)
VIEW_LAYER_TEST(evaluation_selectability_c)
VIEW_LAYER_TEST(evaluation_selectability_d)
VIEW_LAYER_TEST(evaluation_selectability_e)
VIEW_LAYER_TEST(evaluation_selectability_f)
VIEW_LAYER_TEST(group_a)
VIEW_LAYER_TEST(group_b)
VIEW_LAYER_TEST(group_c)
VIEW_LAYER_TEST(group_d)
VIEW_LAYER_TEST(group_e)
VIEW_LAYER_TEST(object_add_cylinder)
VIEW_LAYER_TEST(object_add_empty)
VIEW_LAYER_TEST(object_add_torus)
VIEW_LAYER_TEST(object_add_no_collection_cylinder)
VIEW_LAYER_TEST(object_add_no_collection_empty)
VIEW_LAYER_TEST(object_add_no_collection_torus)
VIEW_LAYER_TEST(object_copy)
VIEW_LAYER_TEST(object_delete_a)
VIEW_LAYER_TEST(object_delete_b)
VIEW_LAYER_TEST(object_link_a)
VIEW_LAYER_TEST(object_link_b)
VIEW_LAYER_TEST(object_link_c)
VIEW_LAYER_TEST(operator_context)
VIEW_LAYER_TEST(make_single_user)
VIEW_LAYER_TEST(move_above_below_scene_collection_a)
VIEW_LAYER_TEST(move_above_below_scene_collection_b)
VIEW_LAYER_TEST(move_above_below_scene_collection_c)
VIEW_LAYER_TEST(move_above_below_scene_collection_d)
VIEW_LAYER_TEST(move_above_below_scene_collection_e)
VIEW_LAYER_TEST(move_above_below_scene_collection_f)
VIEW_LAYER_TEST(move_above_below_scene_collection_g)
VIEW_LAYER_TEST(move_above_below_scene_collection_h)
VIEW_LAYER_TEST(move_above_below_scene_collection_i)
VIEW_LAYER_TEST(move_above_below_scene_collection_sync_a)
VIEW_LAYER_TEST(move_above_below_scene_collection_sync_b)
VIEW_LAYER_TEST(move_above_below_scene_collection_sync_c)
VIEW_LAYER_TEST(move_above_below_scene_collection_sync_d)
VIEW_LAYER_TEST(move_above_below_scene_collection_sync_e)
VIEW_LAYER_TEST(move_above_below_scene_collection_sync_f)
VIEW_LAYER_TEST(move_above_below_scene_collection_sync_g)
VIEW_LAYER_TEST(move_above_below_scene_collection_sync_h)
VIEW_LAYER_TEST(move_above_below_scene_collection_sync_i)
VIEW_LAYER_TEST(move_into_scene_collection_a)
VIEW_LAYER_TEST(move_into_scene_collection_b)
VIEW_LAYER_TEST(move_into_scene_collection_c)
VIEW_LAYER_TEST(move_into_scene_collection_d)
VIEW_LAYER_TEST(move_into_scene_collection_e)
VIEW_LAYER_TEST(move_into_scene_collection_f)
VIEW_LAYER_TEST(move_into_scene_collection_g)
VIEW_LAYER_TEST(move_into_scene_collection_h)
VIEW_LAYER_TEST(move_into_scene_collection_i)
VIEW_LAYER_TEST(move_into_scene_collection_j)
VIEW_LAYER_TEST(move_into_scene_collection_k)
VIEW_LAYER_TEST(move_into_scene_collection_l)
VIEW_LAYER_TEST(move_into_scene_collection_sync_a)
VIEW_LAYER_TEST(move_into_scene_collection_sync_b)
VIEW_LAYER_TEST(move_into_scene_collection_sync_c)
VIEW_LAYER_TEST(move_into_scene_collection_sync_d)
VIEW_LAYER_TEST(move_into_scene_collection_sync_e)
VIEW_LAYER_TEST(move_into_scene_collection_sync_f)
VIEW_LAYER_TEST(move_into_scene_collection_sync_g)
VIEW_LAYER_TEST(move_into_scene_collection_sync_h)
VIEW_LAYER_TEST(move_into_scene_collection_sync_i)
VIEW_LAYER_TEST(move_into_scene_collection_sync_j)
VIEW_LAYER_TEST(move_into_scene_collection_sync_k)
VIEW_LAYER_TEST(move_into_scene_collection_sync_l)
VIEW_LAYER_TEST(move_above_below_layer_collection_a)
VIEW_LAYER_TEST(move_above_below_layer_collection_b)
VIEW_LAYER_TEST(move_above_below_layer_collection_c)
VIEW_LAYER_TEST(move_above_below_layer_collection_d)
VIEW_LAYER_TEST(move_above_below_layer_collection_e)
VIEW_LAYER_TEST(move_above_below_layer_collection_f)
VIEW_LAYER_TEST(move_above_below_layer_collection_g)
VIEW_LAYER_TEST(move_above_below_layer_collection_h)
VIEW_LAYER_TEST(move_above_below_layer_collection_i)
VIEW_LAYER_TEST(move_above_below_layer_collection_j)
VIEW_LAYER_TEST(move_above_below_layer_collection_k)
VIEW_LAYER_TEST(move_above_below_layer_collection_l)
VIEW_LAYER_TEST(move_into_layer_collection_a)
VIEW_LAYER_TEST(move_into_layer_collection_b)
VIEW_LAYER_TEST(move_into_layer_collection_c)
VIEW_LAYER_TEST(move_into_layer_collection_d)
VIEW_LAYER_TEST(move_into_layer_collection_e)
VIEW_LAYER_TEST(move_into_layer_collection_f)
VIEW_LAYER_TEST(move_into_layer_collection_g)
VIEW_LAYER_TEST(move_into_layer_collection_h)
VIEW_LAYER_TEST(move_into_layer_collection_i)
VIEW_LAYER_TEST(move_into_layer_collection_j)
VIEW_LAYER_TEST(layer_linking)
VIEW_LAYER_TEST(layer_syncing)
VIEW_LAYER_TEST(scene_collection_delete)
VIEW_LAYER_TEST(scene_copy_a)
VIEW_LAYER_TEST(scene_copy_b)
VIEW_LAYER_TEST(scene_copy_c)
VIEW_LAYER_TEST(scene_copy_d)
VIEW_LAYER_TEST(scene_copy_e)
VIEW_LAYER_TEST(scene_copy_f)
VIEW_LAYER_TEST(scene_delete)
VIEW_LAYER_TEST(scene_objects)
VIEW_LAYER_TEST(scene_write_read)
VIEW_LAYER_TEST(view_layer_rename)
