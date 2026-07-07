# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""
import modules.ui_test_utils as ui


def wm_toggle_fullscreen():
    e, t, window = ui.test_window()

    # Pre-condition so tests make sense.
    t.assertNotEqual(len(window.screen.areas), 1, "Expected a window with more than one area")

    yield from ui.call_operator(e, "Toggle Maximize Area")
    yield e.ret()
    t.assertEqual(len(window.screen.areas), 1)

    yield from ui.call_operator(e, "Toggle Maximize Area")
    yield e.ret()
    t.assertNotEqual(len(window.screen.areas), 1)


# Checks that opening a temporary file browser exits correctly, as well as exiting a temporary file
# browser on top of a maximized area.
# See: 0a28bb1422
def wm_toggle_stacked_fullscreen_file_browser():
    import bpy

    e, t, window = ui.test_window()

    # Pre-condition so tests make sense.
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
    yield from ui.call_operator(e, "Toggle Maximize Area")
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
    yield from ui.call_operator(e, "Toggle Maximize Area")
    yield e.ret()
    t.assertNotEqual(len(window.screen.areas), 1)


# Checks that stacking a temporary file browser on top of a temporary image editor exits correctly.
# See: ef7fd50f8a, e61588c5a5 (second glitch mentioned there)
def wm_toggle_stacked_fullscreens():
    import bpy

    e, t, window = ui.test_window()

    # Pre-condition so tests make sense.
    t.assertNotEqual(len(window.screen.areas), 1, "Expected a window with more than one area")

    # Ensure temporary file browsers will be opened in a maximized screen.
    bpy.context.preferences.view.filebrowser_display_type = 'SCREEN'
    bpy.context.preferences.view.render_display_type = 'SCREEN'

    initial_area = window.screen.areas[0]

    # Open temporary image editor (would use F12, but better to not involve rendering in tests).
    yield e.f11()
    t.assertEqual(len(window.screen.areas), 1)
    t.assertEqual(window.screen.areas[0].type, 'IMAGE_EDITOR')
    # Create a new image (wouldn't be needed with F12).
    yield e.alt.n()
    yield e.ret()
    yield e.ret()

    # Save image to spawn a temporary file browser.
    yield e.alt.s()
    t.assertEqual(len(window.screen.areas), 1)
    t.assertEqual(window.screen.areas[0].type, 'FILE_BROWSER')

    # Cancel file browser, back to temporary image editor.
    yield e.esc()
    t.assertEqual(len(window.screen.areas), 1)
    t.assertEqual(window.screen.areas[0].type, 'IMAGE_EDITOR')

    # Cancel temporary image editor, back to normal screen.
    yield e.esc()
    t.assertNotEqual(len(window.screen.areas), 1)
    t.assertEqual(window.screen.areas[0], initial_area)

    # Similar test now but other way around: Open a temporary image editor from a temporary file
    # browser. See e61588c5a5 (second glitch mentioned there).

    t.assertNotEqual(window.screen.areas[0].type, 'IMAGE_EDITOR')

    yield e.ctrl.o()
    t.assertEqual(len(window.screen.areas), 1)
    t.assertEqual(window.screen.areas[0].type, 'FILE_BROWSER')

    yield e.f11()
    t.assertEqual(len(window.screen.areas), 1)
    t.assertEqual(window.screen.areas[0].type, 'IMAGE_EDITOR')

    # Cancel temporary image editor, back to temporary file browser.
    yield e.esc()
    t.assertEqual(len(window.screen.areas), 1)
    t.assertEqual(window.screen.areas[0].type, 'FILE_BROWSER')

    # Cancel temporary file browser too, back to normal screen.
    yield e.esc()
    t.assertNotEqual(len(window.screen.areas), 1)
    t.assertEqual(window.screen.areas[0], initial_area)


# See: e61588c5a5 (first glitch mentioned there)
def wm_toggle_temporary_fullscreen_stacked_on_same_type():
    import bpy

    e, t, window = ui.test_window()

    # Pre-condition so tests make sense.
    t.assertNotEqual(len(window.screen.areas), 1, "Expected a window with more than one area")

    # Ensure temporary file browsers will be opened in a maximized screen.
    bpy.context.preferences.view.filebrowser_display_type = 'SCREEN'
    bpy.context.preferences.view.render_display_type = 'SCREEN'

    window.screen.areas[0].type = 'FILE_BROWSER'
    t.assertNotEqual(len(window.screen.areas), 1)

    yield e.ctrl.o()
    t.assertEqual(len(window.screen.areas), 1)
    t.assertEqual(window.screen.areas[0].type, 'FILE_BROWSER')

    # Cancel temporary file browser, check if we're still in a (now normal) file browser.
    yield e.esc()
    t.assertNotEqual(len(window.screen.areas), 1)
    t.assertEqual(window.screen.areas[0].type, 'FILE_BROWSER')
