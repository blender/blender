# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */

import unittest

import bpy
from mathutils import Vector


class TestObjectApi(unittest.TestCase):
    def test_closest_point_on_mesh_of_default_cube(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.mesh.primitive_cube_add()
        ret_val = bpy.context.scene.objects[0].closest_point_on_mesh(Vector((0.0, 0.0, 2.0)))
        self.assertTrue(ret_val[0])
        self.assertEqual(ret_val[1], Vector((0.0, 0.0, 1.0)))


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
