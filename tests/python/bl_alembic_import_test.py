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


class AbstractAlembicTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        'Test dir %s should exist' % self.testdir)

        # Make sure we always start with a known-empty file.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))

    def assertAlmostEqualFloatArray(self, actual, expect, places=6, delta=None):
        """Asserts that the arrays of floats are almost equal."""

        self.assertEqual(len(actual), len(expect),
                         'Actual array has %d items, expected %d' % (len(actual), len(expect)))

        for idx, (act, exp) in enumerate(zip(actual, expect)):
            self.assertAlmostEqual(act, exp, places=places, delta=delta,
                                   msg='%f != %f at index %d' % (act, exp, idx))


class SimpleImportTest(AbstractAlembicTest):
    def test_import_cube_hierarchy(self):
        res = bpy.ops.wm.alembic_import(
            filepath=str(self.testdir / "cubes-hierarchy.abc"),
            as_background_job=False)
        self.assertEqual({'FINISHED'}, res)

        # The objects should be linked to scene.collection in Blender 2.8,
        # and to scene in Blender 2.7x.
        objects = bpy.context.scene.collection.objects
        self.assertEqual(13, len(objects))

        # Test the hierarchy.
        self.assertIsNone(objects['Cube'].parent)
        self.assertEqual(objects['Cube'], objects['Cube_001'].parent)
        self.assertEqual(objects['Cube'], objects['Cube_002'].parent)
        self.assertEqual(objects['Cube'], objects['Cube_003'].parent)
        self.assertEqual(objects['Cube_003'], objects['Cube_004'].parent)
        self.assertEqual(objects['Cube_003'], objects['Cube_005'].parent)
        self.assertEqual(objects['Cube_003'], objects['Cube_006'].parent)

    def test_inherit_or_not(self):
        res = bpy.ops.wm.alembic_import(
            filepath=str(self.testdir / "T52022-inheritance.abc"),
            as_background_job=False)
        self.assertEqual({'FINISHED'}, res)

        # The objects should be linked to scene.collection in Blender 2.8,
        # and to scene in Blender 2.7x.
        objects = bpy.context.scene.collection.objects

        # ABC parent is top-level object, which translates to nothing in Blender
        self.assertIsNone(objects['locator1'].parent)

        # ABC parent is locator1, but locator2 has "inherits Xforms" = false, which
        # translates to "no parent" in Blender.
        self.assertIsNone(objects['locator2'].parent)

        depsgraph = bpy.context.evaluated_depsgraph_get()

        # Shouldn't have inherited the ABC parent's transform.
        loc2 = depsgraph.id_eval_get(objects['locator2'])
        x, y, z = objects['locator2'].matrix_world.to_translation()
        self.assertAlmostEqual(0, x)
        self.assertAlmostEqual(0, y)
        self.assertAlmostEqual(2, z)

        # ABC parent is inherited and translates to normal parent in Blender.
        self.assertEqual(objects['locator2'], objects['locatorShape2'].parent)

        # Should have inherited its ABC parent's transform.
        locshp2 = depsgraph.id_eval_get(objects['locatorShape2'])
        x, y, z = locshp2.matrix_world.to_translation()
        self.assertAlmostEqual(0, x)
        self.assertAlmostEqual(0, y)
        self.assertAlmostEqual(2, z)

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
            self.assertEqual('Cube' in ob.name, ob.select_get())

    def test_change_path_constraint(self):
        import math

        fname = 'cube-rotating1.abc'
        abc = self.testdir / fname
        relpath = bpy.path.relpath(str(abc))

        res = bpy.ops.wm.alembic_import(filepath=str(abc), as_background_job=False)
        self.assertEqual({'FINISHED'}, res)
        cube = bpy.context.active_object

        depsgraph = bpy.context.evaluated_depsgraph_get()

        # Check that the file loaded ok.
        bpy.context.scene.frame_set(10)
        cube = depsgraph.id_eval_get(cube)
        x, y, z = cube.matrix_world.to_euler('XYZ')
        self.assertAlmostEqual(x, 0)
        self.assertAlmostEqual(y, 0)
        self.assertAlmostEqual(z, math.pi / 2, places=5)

        # Change path from absolute to relative. This should not break the animation.
        bpy.context.scene.frame_set(1)
        bpy.data.cache_files[fname].filepath = relpath
        bpy.context.scene.frame_set(10)

        cube = depsgraph.id_eval_get(cube)
        x, y, z = cube.matrix_world.to_euler('XYZ')
        self.assertAlmostEqual(x, 0)
        self.assertAlmostEqual(y, 0)
        self.assertAlmostEqual(z, math.pi / 2, places=5)

        # Replace the Alembic file; this should apply new animation.
        bpy.data.cache_files[fname].filepath = relpath.replace('1.abc', '2.abc')
        depsgraph.update()

        cube = depsgraph.id_eval_get(cube)
        x, y, z = cube.matrix_world.to_euler('XYZ')
        self.assertAlmostEqual(x, math.pi / 2, places=5)
        self.assertAlmostEqual(y, 0)
        self.assertAlmostEqual(z, 0)

    def test_change_path_modifier(self):
        fname = 'animated-mesh.abc'
        abc = self.testdir / fname
        relpath = bpy.path.relpath(str(abc))

        res = bpy.ops.wm.alembic_import(filepath=str(abc), as_background_job=False)
        self.assertEqual({'FINISHED'}, res)
        plane = bpy.context.active_object

        depsgraph = bpy.context.evaluated_depsgraph_get()

        # Check that the file loaded ok.
        bpy.context.scene.frame_set(6)
        scene = bpy.context.scene
        plane_eval = plane.evaluated_get(depsgraph)
        mesh = plane_eval.to_mesh()
        self.assertAlmostEqual(-1, mesh.vertices[0].co.x)
        self.assertAlmostEqual(-1, mesh.vertices[0].co.y)
        self.assertAlmostEqual(0.5905638933181763, mesh.vertices[0].co.z)
        plane_eval.to_mesh_clear()

        # Change path from absolute to relative. This should not break the animation.
        scene.frame_set(1)
        bpy.data.cache_files[fname].filepath = relpath
        scene.frame_set(6)

        plane_eval = plane.evaluated_get(depsgraph)
        mesh = plane_eval.to_mesh()
        self.assertAlmostEqual(1, mesh.vertices[3].co.x)
        self.assertAlmostEqual(1, mesh.vertices[3].co.y)
        self.assertAlmostEqual(0.5905638933181763, mesh.vertices[3].co.z)
        plane_eval.to_mesh_clear()

    def test_import_long_names(self):
        # This file contains very long names. The longest name is 4047 chars.
        bpy.ops.wm.alembic_import(
            filepath=str(self.testdir / "long-names.abc"),
            as_background_job=False)

        self.assertIn('Cube', bpy.data.objects)
        self.assertEqual('CubeShape', bpy.data.objects['Cube'].data.name)


class VertexColourImportTest(AbstractAlembicTest):
    def test_import_from_houdini(self):
        # Houdini saved "face-varying", and as RGB.
        res = bpy.ops.wm.alembic_import(
            filepath=str(self.testdir / "vertex-colours-houdini.abc"),
            as_background_job=False)
        self.assertEqual({'FINISHED'}, res)

        ob = bpy.context.active_object
        layer = ob.data.vertex_colors['Cf']  # MeshLoopColorLayer

        # Test some known-good values.
        self.assertAlmostEqualFloatArray(layer.data[0].color, (0, 0, 0, 1.0))
        self.assertAlmostEqualFloatArray(layer.data[98].color, (0.9019607, 0.4745098, 0.2666666, 1.0))
        self.assertAlmostEqualFloatArray(layer.data[99].color, (0.8941176, 0.4705882, 0.2627451, 1.0))

    def test_import_from_blender(self):
        # Blender saved per-vertex, and as RGBA.
        res = bpy.ops.wm.alembic_import(
            filepath=str(self.testdir / "vertex-colours-blender.abc"),
            as_background_job=False)
        self.assertEqual({'FINISHED'}, res)

        ob = bpy.context.active_object
        layer = ob.data.vertex_colors['Cf']  # MeshLoopColorLayer

        # Test some known-good values.
        self.assertAlmostEqualFloatArray(layer.data[0].color, (1.0, 0.0156862, 0.3607843, 1.0))
        self.assertAlmostEqualFloatArray(layer.data[98].color, (0.0941176, 0.1215686, 0.9137254, 1.0))
        self.assertAlmostEqualFloatArray(layer.data[99].color, (0.1294117, 0.3529411, 0.7529411, 1.0))


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
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
