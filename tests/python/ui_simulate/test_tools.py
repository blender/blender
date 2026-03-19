# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""
import modules.ui_test_utils as ui


def sculpt_mode_toolbar():
    import sys
    e, t, window = ui.test_window()

    # In the default properties area, set it to the tool tab to force access of all
    # tool properties when a tool is activated.
    properties_area = ui.get_window_area_by_type(window, 'PROPERTIES')
    properties_area.spaces[0].context = 'TOOL'

    yield e.ctrl.tab().s()              # Sculpt via pie menu.

    area = ui.get_window_area_by_type(window, 'VIEW_3D')
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
    yield e.five()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.primitive_cube_add")

    yield e.shift.space()
    yield e.six()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.primitive_cone_add")

    yield e.shift.space()
    yield e.seven()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.primitive_cylinder_add")

    yield e.shift.space()
    yield e.eight()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.primitive_uv_sphere_add")

    yield e.shift.space()
    yield e.nine()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.primitive_ico_sphere_add")

    yield e.shift.space()
    yield e.b()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.box_mask")

    yield e.shift.space()
    yield e.shift.three()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.box_hide")

    yield e.shift.space()
    yield e.shift.seven()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.box_face_set")

    yield e.shift.space()
    yield e.ctrl.one()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.box_trim")

    yield e.shift.space()
    yield e.ctrl.five()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.line_project")

    yield e.shift.space()
    yield e.ctrl.six()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.mesh_filter")

    yield e.shift.space()
    yield e.ctrl.seven()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.cloth_filter")

    yield e.shift.space()
    if sys.platform == "darwin":
        # Assigning a keymap entry to Ctrl on MacOS also assigns it to Command. In most cases, either
        # keybind is accepted. However, the toolbar specifically responds to Command, not Ctrl
        yield e.oskey.x()
    else:
        yield e.ctrl.x()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.color_filter")

    yield e.shift.space()
    yield e.ctrl.w()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.face_set_edit")

    yield e.shift.space()
    yield e.ctrl.eight()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.mask_by_color")

    yield e.shift.space()
    yield e.ctrl.nine()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.move")

    yield e.shift.space()
    yield e.ctrl.zero()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.rotate")

    yield e.shift.space()
    yield e.alt.one()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.scale")

    yield e.shift.space()
    yield e.t()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.transform")

    yield e.shift.space()
    yield e.d()
    t.assertEqual(window.workspace.tools.from_space_view3d_mode('SCULPT').idname, "builtin.annotate")
