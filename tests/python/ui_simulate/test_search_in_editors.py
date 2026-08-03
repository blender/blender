# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything; its methods are accessed by run_blender_setup.py.

Tests for search/filter functionality in:
  - Properties editor
  - Outliner
  - Dope Sheet
  - Graph Editor
  - File Browser

Requires: tests/files/ui_tests/test_search_in_editors.blend
  Objects expected in that file:
    __search_test_cube__  - mesh with a Subdivision Surface modifier
"""

import os
import modules.ui_test_utils as ui

_BLEND_FILE = os.path.join(
    os.path.dirname(__file__),
    "..", "..", "files", "ui_tests", "test_search_in_editors.blend",
)


def _set_area_type(area, area_type):
    """Switch *area* to *area_type*."""
    import bpy
    with bpy.context.temp_override(area=area):
        area.type = area_type


def _load_blend():
    """Load the test blend file; re-acquire and return (e, t, window, area)."""
    import bpy
    assert os.path.isfile(_BLEND_FILE), f"Test blend file not found: {_BLEND_FILE}"
    bpy.ops.wm.open_mainfile(filepath=_BLEND_FILE)
    yield  # wait for file load to complete
    e, t, window = ui.test_window()
    area = ui.largest_area(window.screen)
    return e, t, window, area


def test_properties_search():
    """
    Properties editor - Ctrl+F -> 'subdivision'.
    Verifies the search filter matches the Subdivision Surface modifier on
    __search_test_cube__ and that clearing the field resets it.
    """
    import bpy

    e, t, window, area = yield from _load_blend()

    _set_area_type(area, 'PROPERTIES')

    space = area.spaces.active
    t.assertIsInstance(space, bpy.types.SpaceProperties, "Area did not switch to Properties editor")
    t.assertIn("__search_test_cube__", bpy.data.objects, "Blend file is missing __search_test_cube__")

    # Activate the object so its modifier panels are visible.
    bpy.context.view_layer.objects.active = bpy.data.objects["__search_test_cube__"]
    yield

    e.cursor_position_set(*ui.get_area_center(area), move=True)
    yield e.ctrl.f()
    yield e.text("subdivision")
    yield e.ret()
    t.assertEqual(space.search_filter, "subdivision", "Properties: search_filter was not set by Ctrl+F")

    yield e.ctrl.f()
    yield e.back_space()
    yield e.ret()
    t.assertEqual(space.search_filter, "", "Properties: search_filter should be empty after clearing")


def test_outliner_search():
    """
    Outliner - Ctrl+F -> '__search_test_cube__'.
    Verifies filter_text is set and cleared correctly.
    """
    import bpy

    e, t, window, area = yield from _load_blend()

    _set_area_type(area, 'OUTLINER')

    space = area.spaces.active
    t.assertIsInstance(space, bpy.types.SpaceOutliner, "Area did not switch to Outliner")
    t.assertIn("__search_test_cube__", bpy.data.objects, "Blend file is missing __search_test_cube__")

    e.cursor_position_set(*ui.get_area_center(area), move=True)
    yield e.ctrl.f()
    yield e.text("__search_test_cube__")
    yield e.ret()
    t.assertEqual(space.filter_text, "__search_test_cube__", "Outliner: filter_text was not set by Ctrl+F")

    yield e.ctrl.f()
    yield e.back_space()
    yield e.ret()
    t.assertEqual(space.filter_text, "", "Outliner: filter_text was not cleared")


def test_dopesheet_search():
    """
    Dope Sheet - Ctrl+F -> 'location'.
    """
    import bpy

    e, t, window, area = yield from _load_blend()

    _set_area_type(area, 'DOPESHEET_EDITOR')
    with bpy.context.temp_override(area=area):
        area.spaces.active.ui_mode = 'DOPESHEET'
    yield  # wait for mode switch

    space = area.spaces.active
    t.assertIsInstance(space, bpy.types.SpaceDopeSheetEditor, "Area did not switch to Dope Sheet")

    e.cursor_position_set(*ui.get_area_center(area), move=True)
    yield e.ctrl.f()
    yield e.text("location")
    yield e.ret()
    t.assertEqual(space.dopesheet.filter_text, "location",
                  "Dope Sheet: filter_text was not set by Ctrl+F")

    yield e.ctrl.f()
    yield e.back_space()
    yield e.ret()
    t.assertEqual(space.dopesheet.filter_text, "",
                  "Dope Sheet: filter_text was not cleared")


def test_graph_editor_search():
    """
    Graph Editor - Ctrl+F -> 'location'.
    """
    import bpy

    e, t, window, area = yield from _load_blend()

    _set_area_type(area, 'GRAPH_EDITOR')

    space = area.spaces.active
    t.assertIsInstance(space, bpy.types.SpaceGraphEditor, "Area did not switch to Graph Editor")

    e.cursor_position_set(*ui.get_area_center(area), move=True)
    yield e.ctrl.f()
    yield e.text("location")
    yield e.ret()
    t.assertEqual(space.dopesheet.filter_text, "location",
                  "Graph Editor: filter_text was not set by Ctrl+F")

    yield e.ctrl.f()
    yield e.back_space()
    yield e.ret()
    t.assertEqual(space.dopesheet.filter_text, "",
                  "Graph Editor: filter_text was not cleared")


def test_file_browser_search():
    """
    File Browser - Ctrl+F -> 'search_target'.
    """
    import bpy

    e, t, window, area = yield from _load_blend()

    _set_area_type(area, 'FILE_BROWSER')

    space = area.spaces.active
    t.assertIsInstance(space, bpy.types.SpaceFileBrowser, "Area did not switch to File Browser")

    blend_dir = os.path.dirname(_BLEND_FILE)
    with bpy.context.temp_override(area=area):
        bpy.ops.file.select_bookmark(dir=blend_dir)
    yield  # wait for navigation to complete

    params = space.params
    t.assertIsNotNone(params, "File Browser: params is None after navigation")

    e.cursor_position_set(*ui.get_area_center(area), move=True)
    yield e.ctrl.f()
    yield e.text("search_target")
    yield e.ret()
    t.assertEqual(params.filter_search, "search_target",
                  "File Browser: filter_search was not set by Ctrl+F")

    yield e.ctrl.f()
    yield e.back_space()
    yield e.ret()
    t.assertEqual(params.filter_search, "", "File Browser: filter_search was not cleared")
