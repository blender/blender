# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything; its methods are accessed by run_blender_setup.py.

Tests for Info Editor / Reports functionality:
  - Selection modes
  - Copying reports
  - Deleting reports
"""

import modules.ui_test_utils as ui


def _get_window_region(area):
    for region in area.regions:
        if region.type == "WINDOW":
            return region
    assert False, "Info area should have a WINDOW region"


def _setup_info_area():
    import bpy

    bpy.ops.wm.read_homefile(use_empty=True)
    yield

    e, t, window = ui.test_window()
    area = ui.largest_area(window.screen)
    area.type = 'INFO'

    # Perform operations to populate the Info editor with operator reports
    bpy.ops.mesh.primitive_cube_add()
    bpy.ops.transform.translate(value=(1.0, 2.0, 3.0))
    bpy.ops.object.shade_smooth()
    bpy.ops.object.modifier_add(type='BEVEL')
    yield

    return e, t, window, area


def _select_reports(area, action, t):
    import bpy

    region = _get_window_region(area)

    with bpy.context.temp_override(area=area, region=region):
        result = bpy.ops.info.select_all(action=action)

    t.assertEqual(result, {'FINISHED'}, f"{action} selection should complete")
    yield


def _copy_reports(area, t):
    import bpy

    region = _get_window_region(area)

    with bpy.context.temp_override(area=area, region=region):
        result = bpy.ops.info.report_copy()

    t.assertEqual(result, {'FINISHED'}, "Copy reports should complete")
    yield


def _delete_reports(area, t):
    import bpy

    region = _get_window_region(area)

    with bpy.context.temp_override(area=area, region=region):
        result = bpy.ops.info.report_delete()

    t.assertEqual(result, {'FINISHED'}, "Delete reports should complete")
    yield


def test_info_report_selection_modes():
    """
    Verify selection modes.
    """
    import bpy

    e, t, window, area = yield from _setup_info_area()

    space = area.spaces.active
    t.assertIsInstance(space, bpy.types.SpaceInfo)

    yield
    for action in ("SELECT", "DESELECT"):
        yield from _select_reports(area, action, t)
        yield from _copy_reports(area, t)


def test_info_report_delete():
    """
    Verify deleting reports.
    """
    import bpy

    e, t, window, area = yield from _setup_info_area()

    space = area.spaces.active
    t.assertIsInstance(space, bpy.types.SpaceInfo)

    yield from _select_reports(area, 'SELECT', t)
    yield from _delete_reports(area, t)

    t.assertEqual(len(bpy.context.window_manager.reports), 0, "All reports should be deleted")

    # Verify selecting and deleting reports when the reports list is empty.
    yield from _select_reports(area, 'SELECT', t)
    yield from _delete_reports(area, t)


def test_info_report_operator_poll():
    """
    Verify info operators are available.
    """
    import bpy

    e, t, window, area = yield from _setup_info_area()

    region = _get_window_region(area)

    with bpy.context.temp_override(area=area, region=region):
        t.assertTrue(
            bpy.ops.info.select_all.poll(),
            "select_all unavailable",
        )

        t.assertTrue(
            bpy.ops.info.report_copy.poll(),
            "report_copy unavailable",
        )
