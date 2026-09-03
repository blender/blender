# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""

import modules.ui_test_utils as ui


def _find_preferences_window():
    """Find the Preferences window if it exists."""
    import bpy

    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type == "PREFERENCES":
                return window, area

    return None, None


def test_preferences_draw_all():
    """Test that all Preferences sections draw correctly."""
    import bpy
    import addon_utils

    _, t, window = ui.test_window()

    # Open Preferences window.
    with bpy.context.temp_override(window=window):
        bpy.ops.screen.userpref_show()

    yield

    prefs_window, prefs_area = _find_preferences_window()

    t.assertIsNotNone(prefs_window, "Preferences window was not opened")
    t.assertIsNotNone(prefs_area, "Preferences area was not found")

    prefs = bpy.context.preferences
    original_save = prefs.use_preferences_save
    original_show_addons_enabled_only = prefs.view.show_addons_enabled_only
    prefs.use_preferences_save = False
    prefs.view.show_addons_enabled_only = False

    def expand_addons():
        for mod in addon_utils.modules(refresh=False):
            bl_info = addon_utils.module_bl_info(mod)
            if bl_info["show_expanded"]:
                continue

            with bpy.context.temp_override(window=prefs_window, area=prefs_area):
                bpy.ops.preferences.addon_expand(module=mod.__name__)
            yield

    try:
        for item in prefs.bl_rna.properties["active_section"].enum_items:
            with bpy.context.temp_override(window=prefs_window, area=prefs_area):
                prefs.active_section = item.identifier
            yield
            if item.identifier == "ADDONS":
                yield from expand_addons()
    finally:
        prefs.use_preferences_save = original_save
        prefs.view.show_addons_enabled_only = original_show_addons_enabled_only
