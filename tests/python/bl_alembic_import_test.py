# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

"""
./blender.bin --background -noaudio --factory-startup --python tests/python/bl_alembic_import_test.py -- --testdir /path/to/lib/tests/alembic
"""

import pathlib
import sys
import unittest

import bpy

args = None


class SimpleImportTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        'Test dir %s should exist' % self.testdir)

        # Make sure we always start with a known-empty file.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))

    def test_import_cube_hierarchy(self):
        res = bpy.ops.wm.alembic_import(
            filepath=str(self.testdir / "cubes-hierarchy.abc"),
            as_background_job=False)
        self.assertEqual({'FINISHED'}, res)

        # The objects should be linked to scene_collection in Blender 2.8,
        # and to scene in Blender 2.7x.
        objects = bpy.context.scene.objects
        self.assertEqual(13, len(objects))

        # Test the hierarchy.
        self.assertIsNone(objects['Cube'].parent)
        self.assertEqual(objects['Cube'], objects['Cube_001'].parent)
        self.assertEqual(objects['Cube'], objects['Cube_002'].parent)
        self.assertEqual(objects['Cube'], objects['Cube_003'].parent)
        self.assertEqual(objects['Cube_003'], objects['Cube_004'].parent)
        self.assertEqual(objects['Cube_003'], objects['Cube_005'].parent)
        self.assertEqual(objects['Cube_003'], objects['Cube_006'].parent)

    def test_select_after_import(self):
        # Add a sphere, so that there is something in the scene, selected, and active,
        # before we do the Alembic import.
        bpy.ops.mesh.primitive_uv_sphere_add()
        sphere = bpy.context.active_object
        self.assertEqual('Sphere', sphere.name)
        self.assertEqual([sphere], bpy.context.selected_objects)

        bpy.ops.wm.alembic_import(
            filepath=str(self.testdir / "cubes-hierarchy.abc"),
            as_background_job=False)

        # The active object is probably the first one that was imported, but this
        # behaviour is not defined. At least it should be one of the cubes, and
        # not the sphere.
        self.assertNotEqual(sphere, bpy.context.active_object)
        self.assertTrue('Cube' in bpy.context.active_object.name)

        # All cubes should be selected, but the sphere shouldn't be.
        for ob in bpy.data.objects:
            self.assertEqual('Cube' in ob.name, ob.select)

    def test_change_path(self):
        import math

        fname = 'cube-rotating1.abc'
        abc = self.testdir / fname
        relpath = bpy.path.relpath(str(abc))

        res = bpy.ops.wm.alembic_import(filepath=str(abc), as_background_job=False)
        self.assertEqual({'FINISHED'}, res)
        cube = bpy.context.active_object

        # Check that the file loaded ok.
        bpy.context.scene.frame_set(10)
        x, y, z = cube.matrix_world.to_euler('XYZ')
        self.assertAlmostEqual(x, 0)
        self.assertAlmostEqual(y, 0)
        self.assertAlmostEqual(z, math.pi / 2, places=5)

        # Change path from absolute to relative. This should not break the animation.
        bpy.context.scene.frame_set(1)
        bpy.data.cache_files[fname].filepath = relpath
        bpy.context.scene.frame_set(10)

        x, y, z = cube.matrix_world.to_euler('XYZ')
        self.assertAlmostEqual(x, 0)
        self.assertAlmostEqual(y, 0)
        self.assertAlmostEqual(z, math.pi / 2, places=5)

        # Replace the Alembic file; this should apply new animation.
        bpy.data.cache_files[fname].filepath = relpath.replace('1.abc', '2.abc')
        bpy.context.scene.update()

        x, y, z = cube.matrix_world.to_euler('XYZ')
        self.assertAlmostEqual(x, math.pi / 2, places=5)
        self.assertAlmostEqual(y, 0)
        self.assertAlmostEqual(z, 0)


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--')+1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    import traceback
    # So a python error exits Blender itself too
    try:
        main()
    except SystemExit:
        raise
    except:
        traceback.print_exc()
        sys.exit(1)
