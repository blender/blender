# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import unittest


class CompositingNodeGroupTest(unittest.TestCase):
    """
    Tests specific to the root compositing node tree
    """

    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)

    def test_assign_invalid(self):
        scene = bpy.data.scenes["Scene"]
        with self.assertRaises(RuntimeError):
            scene.compositing_node_group = bpy.data.node_groups.new("invalid", "GeometryNodeTree")


if __name__ == "__main__":
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
