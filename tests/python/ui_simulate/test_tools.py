# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""


def _test_window(windows_exclude=None):
    import bpy
    wm = bpy.data.window_managers[0]
    if windows_exclude is None:
        return wm.windows[0]
    for window in wm.windows:
        if window not in windows_exclude:
            return window
    return None


def _test_vars(window):
    import unittest
    from modules.easy_keys import EventGenerate
    return (
        EventGenerate(window),
        unittest.TestCase(),
    )


def _window_area_get_by_type(window, space_type):
    for area in window.screen.areas:
        if area.type == space_type:
            return area


def sculpt_mode_toolbar():
    e, t = _test_vars(window := _test_window())

    # In the default properties area, set it to the tool tab to force access of all
    # tool properties when a tool is activated.
    properties_area = _window_area_get_by_type(window, 'PROPERTIES')
    properties_area.spaces[0].context = 'TOOL'

    yield e.ctrl.tab().s()              # Sculpt via pie menu.

    area = _window_area_get_by_type(window, 'VIEW_3D')
    position = (area.x + int(area.width * 0.05), area.y + area.height // 2)
    e.cursor_position_set(*position, move=True)     # Move mouse over the toolbar

    yield e.shift.space()
    yield e.one()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.brush")

    yield e.shift.space()
    yield e.two()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin_brush.paint")

    yield e.shift.space()
    yield e.three()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin_brush.mask")

    yield e.shift.space()
    yield e.four()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin_brush.draw_face_sets")

    yield e.shift.space()
    yield e.b()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.box_mask")

    yield e.shift.space()
    yield e.eight()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.box_hide")

    yield e.shift.space()
    yield e.shift.two()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.box_face_set")

    yield e.shift.space()
    yield e.shift.six()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.box_trim")

    yield e.shift.space()
    yield e.shift.zero()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.line_project")

    yield e.shift.space()
    yield e.ctrl.one()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.mesh_filter")

    yield e.shift.space()
    yield e.ctrl.two()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.cloth_filter")

    yield e.shift.space()
    yield e.ctrl.three()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.color_filter")

    yield e.shift.space()
    yield e.ctrl.w()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.face_set_edit")

    yield e.shift.space()
    yield e.ctrl.four()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.mask_by_color")

    yield e.shift.space()
    yield e.ctrl.five()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.move")

    yield e.shift.space()
    yield e.ctrl.six()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.rotate")

    yield e.shift.space()
    yield e.ctrl.seven()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.scale")

    yield e.shift.space()
    yield e.t()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.transform")

    yield e.shift.space()
    yield e.d()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.annotate")
