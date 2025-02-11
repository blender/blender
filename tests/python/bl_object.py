import unittest

import bpy
from mathutils import Vector


class TestObjectApi(unittest.TestCase):
    def test_closest_point_on_mesh_of_default_cube(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete(use_global=False)
        bpy.ops.outliner.orphans_purge()
        bpy.ops.mesh.primitive_cube_add()
        ret_val = bpy.context.scene.objects[0].closest_point_on_mesh(Vector((0.0, 0.0, 2.0)))
        self.assertTrue(ret_val[0])
        self.assertEqual(ret_val[1], Vector((0.0, 0.0, 1.0)))


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
