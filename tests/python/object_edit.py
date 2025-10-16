# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


import unittest
import bpy
import sys


class TestObjectEdit(unittest.TestCase):

    def setUp(self):
        if bpy.context.object and bpy.context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        for mesh in [mesh for mesh in bpy.data.meshes]:
            bpy.data.meshes.remove(mesh)
        for ob in [ob for ob in bpy.data.objects]:
            bpy.data.objects.remove(ob)

    def test_auto_smooth_detection(self):
        bpy.ops.mesh.primitive_cube_add()
        ob = bpy.context.object
        bpy.ops.object.shade_auto_smooth(use_auto_smooth=True)
        bpy.ops.object.shade_auto_smooth(use_auto_smooth=True)
        bpy.ops.object.shade_auto_smooth(use_auto_smooth=True)
        bpy.ops.object.shade_auto_smooth(use_auto_smooth=True)
        self.assertEqual(len(ob.modifiers), 1)
        bpy.ops.object.shade_flat()
        self.assertEqual(len(ob.modifiers), 0)
        bpy.ops.object.shade_auto_smooth(use_auto_smooth=True)
        bpy.ops.object.shade_smooth()
        self.assertEqual(len(ob.modifiers), 0)


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
