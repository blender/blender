# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""
import os

import modules.ui_test_utils as ui


def compositor_modifier_mask_with_single_input():
    import bpy

    _, t, window = ui.test_window()

    scene = window.scene
    window.workspace.sequencer_scene = scene
    sequence_editor = scene.sequence_editor_create()
    movie_path = os.path.normpath(
        os.path.join(os.path.dirname(__file__), "..", "..", "files", "compositor", "motion.mp4")
    )
    strip = sequence_editor.strips.new_movie(
        name="Movie",
        filepath=movie_path,
        channel=1,
        frame_start=1,
    )
    strip.select = True
    sequence_editor.active_strip = strip

    node_group = bpy.data.node_groups.new("Compositor Nodes", 'CompositorNodeTree')
    node_group.interface.new_socket(name="Image", in_out='INPUT', socket_type='NodeSocketColor')
    node_group.interface.new_socket(name="Image", in_out='OUTPUT', socket_type='NodeSocketColor')
    group_input = node_group.nodes.new('NodeGroupInput')
    group_output = node_group.nodes.new('NodeGroupOutput')
    node_group.links.new(group_input.outputs["Image"], group_output.inputs["Image"])
    interface_items = node_group.interface.items_tree
    t.assertEqual(sum(item.in_out == 'INPUT' for item in interface_items), 1)
    t.assertEqual(sum(item.in_out == 'OUTPUT' for item in interface_items), 1)

    modifier = strip.modifiers.new("Compositor", 'COMPOSITOR')
    modifier.node_group = node_group
    modifier.show_expanded = True
    mask = bpy.data.masks.new("Mask")

    properties_area = ui.get_window_area_by_type(window, 'PROPERTIES')
    yield
    properties_area.spaces.active.context = 'STRIP_MODIFIER'
    yield
    t.assertEqual(properties_area.spaces.active.context, 'STRIP_MODIFIER')

    modifier.input_mask_type = 'ID'
    modifier.input_mask_id = mask
    properties_area.tag_redraw()
    yield

    t.assertEqual(modifier.input_mask_type, 'ID')
    t.assertIs(modifier.input_mask_id, mask)
