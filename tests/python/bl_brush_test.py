# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import sys
import unittest

import bpy


class AssetActivateTest(unittest.TestCase):
    def setUp(self):
        # Test case isn't specific to Sculpt Mode, but we need a paint mode in general.
        bpy.ops.object.mode_set(mode='SCULPT')
        bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_sculpt.blend/Brush/Draw')

    def test_loads_essential_asset(self):
        result = bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_sculpt.blend/Brush/Smooth')
        self.assertEqual({'FINISHED'}, result)

    def test_toggle_when_brush_differs_sets_specified_brush(self):
        """Test that using the 'Toggle' parameter when the brush is not active still activates the correct brush"""
        bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_sculpt.blend/Brush/Mask',
            use_toggle=True)
        self.assertEqual(bpy.context.tool_settings.sculpt.brush.name, 'Mask')

    def test_toggle_when_brush_matches_sets_previous_brush(self):
        """Test that using the 'Toggle' parameter when the brush is active activates the previously activated brush"""
        bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_sculpt.blend/Brush/Mask',
            use_toggle=True)
        self.assertEqual(bpy.context.tool_settings.sculpt.brush.name, 'Mask')
        bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_sculpt.blend/Brush/Mask',
            use_toggle=True)
        self.assertEqual(bpy.context.tool_settings.sculpt.brush.name, 'Draw')


if __name__ == "__main__":
    # Drop all arguments before "--", or everything if the delimiter is absent. Keep the executable path.
    unittest.main(argv=sys.argv[:1] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []))
