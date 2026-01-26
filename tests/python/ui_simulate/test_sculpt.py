# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""
import modules.ui_test_utils as ui


def asset_shelf_brush_selection():
    e, t, window = ui.test_window()

    yield e.shift.f5()                              # 3D Viewport.
    yield e.ctrl.alt.space()                        # Full-screen.
    yield e.ctrl.tab().s()                          # Sculpt via pie menu.

    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    # We use this hardcoded area percent position because the asset shelf is very large, this centers the cursor
    # in the correct position.
    position = (area.x + int(area.width * 0.30), area.y + area.height // 2)
    e.cursor_position_set(*position, move=True)     # Move mouse
    yield

    yield e.shift.space()                           # Asset Shelf
    yield e.text("Blob")                            # Search for "Blob"
    yield e.esc()

    # We repeat this again because the asset shelf is too large to fit on the screen the first time...
    yield e.shift.space()                           # Asset Shelf

    e.leftmouse.tap()
    yield

    import bpy
    current_brush = bpy.context.tool_settings.sculpt.brush
    t.assertEqual(current_brush.name, "Blob")


def _view3d_startup_area_maximized(e):
    """
    Set the 3D viewport and set the area full-screen so no other regions.
    """
    yield e.shift.f5()                  # 3D Viewport.
    yield e.ctrl.alt.space()            # Full-screen.
    yield e.a()                         # Select all.
    yield e.delete().ret()              # Delete all.


def _subdivide_mesh(e, times):
    yield e.tab()                       # Enter Edit mode.
    for i in range(times):
        yield e.ctrl.e().d()                # Subdivide.
    yield e.tab()                       # Leave Edit mode.


def _cursor_motion_data_x(e, start_position, pixels):
    for x in range(pixels // 10):
        position = (start_position[0] + x * 10, start_position[1])
        yield e.cursor_position_set(*position, move=True)


def _num_fully_masked_vertices():
    import bpy
    import numpy as np

    mesh = bpy.context.object.data

    if not mesh.attributes.get('.sculpt_mask'):
        return 0

    mask_attr = mesh.attributes['.sculpt_mask']

    num_vertices = mesh.attributes.domain_size('POINT')

    mask_data = np.zeros(num_vertices, dtype=np.float32)
    mask_attr.data.foreach_get('value', mask_data)

    return np.count_nonzero(mask_data == 1.0)


def mask_expand_and_invert():
    e, t, window = ui.test_window()
    yield from _view3d_startup_area_maximized(e)

    yield from ui.call_menu(e, "Add -> Mesh -> Monkey")
    yield e.numpad_period()                                     # View monkey

    yield from _subdivide_mesh(e, 3)

    yield e.ctrl.tab().s()                                      # Sculpt via pie menu.

    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    position = (area.x + area.width // 2, area.y + area.height // 2)
    yield e.cursor_position_set(*position, move=True)           # Move mouse to center

    yield e.shift.a()                                           # Expand operator

    yield from _cursor_motion_data_x(e, position, 200)
    e.leftmouse.tap()
    yield

    initial_masked_verts = _num_fully_masked_vertices()
    t.assertEqual(initial_masked_verts, 8548)

    yield e.a()                                                 # Mask pie menu
    yield e.i()                                                 # Invert

    inverted_masked_verts = _num_fully_masked_vertices()
    t.assertEqual(inverted_masked_verts, 22598)

    import bpy
    mesh = bpy.context.object.data
    total_verts = mesh.attributes.domain_size('POINT')
    t.assertEqual(initial_masked_verts + inverted_masked_verts, total_verts)
