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


def _call_by_name(e, text: str):
    yield e.f3()
    yield e.text(text)
    yield e.ret()


def _call_menu(e, text: str):
    yield e.f3()
    yield e.text_unicode(text.replace(" -> ", " \u25b8 "))
    yield e.ret()


def _cursor_position_from_area(area):
    return (
        area.x + area.width // 2,
        area.y + area.height // 2,
    )


def _window_area_get_by_type(window, space_type):
    for area in window.screen.areas:
        if area.type == space_type:
            return area


def _cursor_position_from_spacetype(window, space_type):
    area = _window_area_get_by_type(window, space_type)
    if area is None:
        raise Exception("Space Type {!r} not found".format(space_type))
    return _cursor_position_from_area(area)


def asset_shelf_brush_selection():
    e, t = _test_vars(window := _test_window())

    yield e.shift.f5()                              # 3D Viewport.
    yield e.ctrl.alt.space()                        # Full-screen.
    yield e.ctrl.tab().s()                          # Sculpt via pie menu.

    area = _window_area_get_by_type(window, 'VIEW_3D')
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
