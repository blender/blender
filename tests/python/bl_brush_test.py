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

    def test_loads_essential_asset(self):
        result = bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_sculpt.blend/Brush/Smooth')
        self.assertEqual({'FINISHED'}, result)


if __name__ == "__main__":
    # Drop all arguments before "--", or everything if the delimiter is absent. Keep the executable path.
    unittest.main(argv=sys.argv[:1] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []))
