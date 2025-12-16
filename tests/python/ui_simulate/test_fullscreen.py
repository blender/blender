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


def wm_toggle_fullscreen():
    e, t = _test_vars(window := _test_window())

    t.assertNotEqual(len(window.screen.areas), 1, "Expected a window with more than one area")

    yield from _call_by_name(e, "Toggle Maximize Area")
    yield e.ret()
    t.assertEqual(len(window.screen.areas), 1)

    yield from _call_by_name(e, "Toggle Maximize Area")
    yield e.ret()
    t.assertNotEqual(len(window.screen.areas), 1)


def wm_toggle_stacked_fullscreen_file_browser():
    import bpy

    e, t = _test_vars(window := _test_window())

    t.assertNotEqual(len(window.screen.areas), 1, "Expected a window with more than one area")

    # Ensure temporary file browsers will be opened in a maximized screen.
    bpy.context.preferences.view.filebrowser_display_type = 'SCREEN'

    # Open temporary file browser.
    yield e.ctrl.o()
    t.assertEqual(len(window.screen.areas), 1)
    t.assertEqual(window.screen.areas[0].type, 'FILE_BROWSER')

    # Escape should leave the temporary file browser maximized screen.
    yield e.esc()
    t.assertNotEqual(len(window.screen.areas), 1)
    t.assertNotEqual(window.screen.areas[0].type, 'FILE_BROWSER')

    # Maximize a normal area.
    yield from _call_by_name(e, "Toggle Maximize Area")
    yield e.ret()
    t.assertEqual(len(window.screen.areas), 1)
    t.assertNotEqual(window.screen.areas[0].type, 'FILE_BROWSER')

    # Open a file browser in a stacked fullscreen.
    yield e.ctrl.o()
    t.assertEqual(len(window.screen.areas), 1)
    t.assertEqual(window.screen.areas[0].type, 'FILE_BROWSER')

    # Pressing 'Escape' should return to the previous full screen.
    yield e.esc()
    t.assertEqual(len(window.screen.areas), 1)
    t.assertNotEqual(window.screen.areas[0].type, 'FILE_BROWSER')
    restored_area = window.screen.areas[0]

    # Pressing 'Escape' again shouldn't cause any change.
    yield e.esc()
    t.assertEqual(len(window.screen.areas), 1)
    t.assertNotEqual(window.screen.areas[0].type, 'FILE_BROWSER')
    t.assertEqual(restored_area, window.screen.areas[0])

    # Restore to non-maximized area.
    yield from _call_by_name(e, "Toggle Maximize Area")
    yield e.ret()
    t.assertNotEqual(len(window.screen.areas), 1)
