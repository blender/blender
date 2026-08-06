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


def _reset_objects(e):
    yield e.ctrl.tab().o()              # Object Mode
    yield e.a()                         # Select all.
    yield e.delete().ret()              # Delete all.


def _subdivide_mesh(e, times):
    yield e.tab()                       # Enter Edit mode.
    for i in range(times):
        yield e.ctrl.e().d()            # Subdivide.
    yield e.tab()                       # Leave Edit mode.


def _create_test_monkey(e):
    yield from ui.call_menu(e, "Add -> Mesh -> Monkey")
    yield e.numpad_period()                                     # View monkey

    yield from _subdivide_mesh(e, 3)

    yield e.ctrl.tab().s()                                      # Sculpt via pie menu.


def _num_matching_face_set(face_set_id):
    import bpy
    import numpy as np

    mesh = bpy.context.object.data

    if not mesh.attributes.get('.sculpt_face_set'):
        return 0

    face_set_attr = mesh.attributes['.sculpt_face_set']

    num_faces = mesh.attributes.domain_size('FACE')

    face_set_data = np.zeros(num_faces, dtype=np.int32)
    face_set_attr.data.foreach_get('value', face_set_data)

    return np.count_nonzero(face_set_data == face_set_id)


def _assertBetween(t, value, low, high):
    t.assertGreaterEqual(value, low)
    t.assertLessEqual(value, high)


def face_set_expand():
    import bpy
    e, t, window = ui.test_window()
    yield from _view3d_startup_area_maximized(e)

    yield from _create_test_monkey(e)

    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    position = (area.x + area.width // 2, area.y + area.height // 2)
    yield e.cursor_position_set(*position, move=True)           # Move mouse to center

    yield e.shift.w()                                           # Expand operator

    yield from _move_horizontal(e, position, 200)
    e.leftmouse.tap()
    yield

    non_default_faces = _num_matching_face_set(2)
    _assertBetween(t, non_default_faces, 8000, 9000)

    default_faces = _num_matching_face_set(1)
    mesh = bpy.context.object.data
    num_faces = mesh.attributes.domain_size('FACE')
    t.assertEqual(default_faces + non_default_faces, num_faces)


def face_set_gestures():
    e, t, window = ui.test_window()
    yield from _view3d_startup_area_maximized(e)

    # Box Face Set
    yield from _create_test_monkey(e)
    yield from ui.call_operator(e, "Box Face Set")
    yield from e.leftmouse.cursor_motion(ui.cursor_motion_data_xy(window))
    _assertBetween(t, _num_matching_face_set(2), 29500, 30500)
    yield from _reset_objects(e)

    # Lasso Face Set
    yield from _create_test_monkey(e)
    yield from ui.call_operator(e, "Lasso Face Set")
    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')
    yield from e.leftmouse.cursor_motion(ui.cursor_motion_data_circle(center, 100))
    _assertBetween(t, _num_matching_face_set(2), 8000, 9000)
    yield from _reset_objects(e)

    # Line Face Set
    yield from _create_test_monkey(e)
    yield from ui.call_operator(e, "Line Face Set")
    yield from e.leftmouse.cursor_motion(ui.cursor_motion_data_y(window))
    _assertBetween(t, _num_matching_face_set(2), 6000, 7000)


def _num_hidden_vertices():
    import bpy
    import numpy as np

    mesh = bpy.context.object.data

    if not mesh.attributes.get('.hide_vert'):
        return 0

    hide_attr = mesh.attributes['.hide_vert']

    num_vertices = mesh.attributes.domain_size('POINT')

    hide_data = np.zeros(num_vertices, dtype=np.bool)
    hide_attr.data.foreach_get('value', hide_data)

    return np.count_nonzero(hide_data)


def hide_gestures():
    e, t, window = ui.test_window()
    yield from _view3d_startup_area_maximized(e)

    # Box Hide
    yield from _create_test_monkey(e)
    t.assertEqual(_num_hidden_vertices(), 0)
    yield from ui.call_operator(e, "Box Hide")
    yield from e.leftmouse.cursor_motion(ui.cursor_motion_data_xy(window))
    _assertBetween(t, _num_hidden_vertices(), 28500, 29500)
    yield from _reset_objects(e)

    # Lasso Hide
    yield from _create_test_monkey(e)
    t.assertEqual(_num_hidden_vertices(), 0)
    yield from ui.call_operator(e, "Lasso Hide")
    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')
    yield from e.leftmouse.cursor_motion(ui.cursor_motion_data_circle(center, 100))
    _assertBetween(t, _num_hidden_vertices(), 8000, 9000)
    yield from _reset_objects(e)

    # Line Hide
    yield from _create_test_monkey(e)
    t.assertEqual(_num_hidden_vertices(), 0)
    yield from ui.call_operator(e, "Line Hide")
    yield from e.leftmouse.cursor_motion(ui.cursor_motion_data_y(window))
    _assertBetween(t, _num_hidden_vertices(), 6000, 7000)


def _move_horizontal(e, start_position, pixels):
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

    yield from _create_test_monkey(e)

    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    position = (area.x + area.width // 2, area.y + area.height // 2)
    yield e.cursor_position_set(*position, move=True)           # Move mouse to center

    yield e.shift.a()                                           # Expand operator

    yield from _move_horizontal(e, position, 200)
    e.leftmouse.tap()
    yield

    initial_masked_verts = _num_fully_masked_vertices()
    _assertBetween(t, initial_masked_verts, 8000, 9000)

    yield e.a()                                                 # Mask pie menu
    yield e.i()                                                 # Invert

    inverted_masked_verts = _num_fully_masked_vertices()

    import bpy
    mesh = bpy.context.object.data
    total_verts = mesh.attributes.domain_size('POINT')
    t.assertEqual(initial_masked_verts + inverted_masked_verts, total_verts)


def mask_gestures():
    e, t, window = ui.test_window()
    yield from _view3d_startup_area_maximized(e)

    # Box Mask
    yield from _create_test_monkey(e)
    t.assertEqual(_num_fully_masked_vertices(), 0)
    yield from ui.call_operator(e, "Box Mask")
    yield from e.leftmouse.cursor_motion(ui.cursor_motion_data_xy(window))
    _assertBetween(t, _num_fully_masked_vertices(), 28500, 29500)
    yield from _reset_objects(e)

    # Lasso Mask
    yield from _create_test_monkey(e)
    t.assertEqual(_num_fully_masked_vertices(), 0)
    yield from ui.call_operator(e, "Lasso Mask")
    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')
    yield from e.leftmouse.cursor_motion(ui.cursor_motion_data_circle(center, 100))
    _assertBetween(t, _num_fully_masked_vertices(), 8000, 9000)
    yield from _reset_objects(e)

    # Line Mask
    yield from _create_test_monkey(e)
    t.assertEqual(_num_fully_masked_vertices(), 0)
    yield from ui.call_operator(e, "Line Mask")
    yield from e.leftmouse.cursor_motion(ui.cursor_motion_data_y(window))
    _assertBetween(t, _num_fully_masked_vertices(), 6000, 7000)


def _create_test_cube(e):
    yield from ui.call_menu(e, "Add -> Mesh -> Cube")
    yield e.numpad_period()                                     # View Cube

    yield e.ctrl.tab().s()                                      # Sculpt via pie menu.


def trim_gestures():
    import bpy
    e, t, window = ui.test_window()
    yield from _view3d_startup_area_maximized(e)

    # Box Trim
    yield from _create_test_cube(e)
    mesh = bpy.context.object.data
    t.assertEqual(mesh.attributes.domain_size('POINT'), 8)
    yield from ui.call_operator(e, "Box Trim")
    yield from e.leftmouse.cursor_motion(ui.cursor_motion_data_xy(window))
    t.assertEqual(mesh.attributes.domain_size('POINT'), 28)
    yield from _reset_objects(e)

    # Lasso Trim
    yield from _create_test_cube(e)
    mesh = bpy.context.object.data
    t.assertEqual(mesh.attributes.domain_size('POINT'), 8)
    yield from ui.call_operator(e, "Lasso Trim")
    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')
    yield from e.leftmouse.cursor_motion(ui.cursor_motion_data_circle(center, 100))
    _assertBetween(t, mesh.attributes.domain_size('POINT'), 85, 95)
    yield from _reset_objects(e)

    # Line Trim
    yield from _create_test_cube(e)
    mesh = bpy.context.object.data
    t.assertEqual(mesh.attributes.domain_size('POINT'), 8)
    yield from ui.call_operator(e, "Line Trim")
    yield from e.leftmouse.cursor_motion(ui.cursor_motion_data_y(window))
    t.assertEqual(mesh.attributes.domain_size('POINT'), 10)


def primitive_tool_add():
    import bpy
    e, t, window = ui.test_window()

    yield e.ctrl.tab().s()                                                # Sculpt via pie menu.

    # The tool is not callable from the F3 menu, as it only sets the active tool instead of activating the
    # gizmo, work around this by using the toolbar to activate it instead.
    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    position = (area.x + int(area.width * 0.05), area.y + area.height // 2)
    e.cursor_position_set(*position, move=True)                           # Move mouse over the toolbar
    yield e.shift.space()
    yield e.five()                                                        # Add Cube

    position = (area.x + area.width // 2, area.y + area.height // 2)
    yield e.cursor_position_set(*position, move=True)                     # Move mouse to center

    e.leftmouse.press()
    yield

    pixels = 10
    for delta in range(pixels):
        position = (position[0] + delta, position[1] + delta)
        yield e.cursor_position_set(*position, move=True)

    e.leftmouse.release()
    yield

    for delta in range(pixels):
        position = (position[0] - delta, position[1] - delta)
        yield e.cursor_position_set(*position, move=True)

    e.leftmouse.tap()
    yield

    mesh = bpy.context.object.data
    num_faces = mesh.attributes.domain_size('FACE')
    t.assertEqual(num_faces, 12)                                          # There should be 6 + 6 faces
