# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */

import unittest

import bpy
from mathutils import Vector


class ClosestPointOnMeshTest(unittest.TestCase):
    def test_function_finds_closest_point_successfully(self):
        """Test that attempting to find the closest point succeeds and returns the correct location."""

        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.mesh.primitive_cube_add()
        ret_val = bpy.context.scene.objects[0].closest_point_on_mesh(Vector((0.0, 0.0, 2.0)))
        self.assertTrue(ret_val[0])
        self.assertEqual(ret_val[1], Vector((0.0, 0.0, 1.0)))


class RemeshTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.ed.undo_push()

        bpy.ops.mesh.primitive_cube_add()
        bpy.ops.sculpt.sculptmode_toggle()

    def test_operator_remeshes_basic_cube(self):
        """Test that using the operator with default settings creates a mesh with the expected amount of vertices."""
        mesh = bpy.context.object.data
        mesh.remesh_voxel_size = 0.1

        ret_val = bpy.ops.object.voxel_remesh()

        self.assertEqual({'FINISHED'}, ret_val)

        num_vertices = mesh.attributes.domain_size('POINT')
        self.assertEqual(num_vertices, 2648)

    def test_operator_doesnt_run_with_0_voxel_size(self):
        """Test that using the operator returns an error to the user with a voxel size of 0."""
        mesh = bpy.context.object.data
        mesh.remesh_voxel_size = 0

        with self.assertRaises(RuntimeError):
            bpy.ops.object.voxel_remesh()


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
