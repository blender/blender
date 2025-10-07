# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
./blender.bin --background --factory-startup --python tests/python/bl_alembic_io_test.py -- --testdir /path/to/tests/files/alembic
"""

import math
import pathlib
import sys
import tempfile
import unittest

import bpy

sys.path.append(str(pathlib.Path(__file__).parent.absolute()))

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
        # behavior is not defined. At least it should be one of the cubes, and
        # not the sphere.
        self.assertNotEqual(sphere, bpy.context.active_object)
        self.assertTrue('Cube' in bpy.context.active_object.name)

        # All cubes should be selected, but the sphere shouldn't be.
        for ob in bpy.data.objects:
            self.assertEqual('Cube' in ob.name, ob.select_get())

    def test_change_path_constraint(self):
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


class CameraExportImportTest(unittest.TestCase):
    names = [
        'CAM_Unit_Transform',
        'CAM_Look_+Y',
        'CAM_Static_Child_Left',
        'CAM_Static_Child_Right',
        'Static_Child',
        'CAM_Animated',
        'CAM_Animated_Child_Left',
        'CAM_Animated_Child_Right',
        'Animated_Child',
    ]

    def setUp(self):
        self._tempdir = tempfile.TemporaryDirectory()
        self.tempdir = pathlib.Path(self._tempdir.name)

    def tearDown(self):
        # Unload the current blend file to release the imported Alembic file.
        # This is necessary on Windows in order to be able to delete the
        # temporary ABC file.
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        self._tempdir.cleanup()

    def test_export_hierarchy(self):
        self.do_export_import_test(flatten=False)

        # Double-check that the export was hierarchical.
        objects = bpy.context.scene.collection.objects
        for name in self.names:
            if 'Child' in name:
                self.assertIsNotNone(objects[name].parent)
            else:
                self.assertIsNone(objects[name].parent)

    def test_export_flattened(self):
        self.do_export_import_test(flatten=True)

        # Double-check that the export was flat.
        objects = bpy.context.scene.collection.objects
        for name in self.names:
            self.assertIsNone(objects[name].parent)

    def test_mesh_subd_varying(self):
        """Test meshes with subdivision crease values varying over time."""

        abc_path = str(self.tempdir / "mesh_subd_varying.abc")

        # Export
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "mesh_subd_varying.blend"))
        self.assertIn('FINISHED', bpy.ops.wm.alembic_export(
            filepath=abc_path,
            subdiv_schema=True
        ))

        # Re-import what we just exported into an empty file.
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "empty.blend"))
        bpy.ops.wm.alembic_import(
            filepath=abc_path,
            as_background_job=False)

        #
        # Validate Mesh data
        #
        blender_mesh1 = bpy.data.objects["mesh_edge_crease"]
        blender_mesh2 = bpy.data.objects["mesh_vert_crease"]

        # A MeshSequenceCache modifier should be present on every imported object
        for blender_mesh in [blender_mesh1, blender_mesh2]:
            self.assertTrue(len(blender_mesh.modifiers) == 1 and blender_mesh.modifiers[0].type ==
                            'MESH_SEQUENCE_CACHE', f"{blender_mesh.name} has incorrect modifiers")

        # Conversion from USD to Blender convention
        def sharpness_to_crease(sharpness):
            return math.sqrt(sharpness * 0.1)

        # Compare Blender data against the expected value for every frame
        for frame in range(1, 25):
            bpy.context.scene.frame_set(frame)
            depsgraph = bpy.context.evaluated_depsgraph_get()
            blender_mesh1_eval = bpy.data.objects["mesh_edge_crease"].evaluated_get(depsgraph)
            blender_mesh2_eval = bpy.data.objects["mesh_vert_crease"].evaluated_get(depsgraph)

            # The file was written using a simple formula for each frame's crease value
            expected_edge_creases = [round(frame / 24.0, 3)] * 12
            expected_vert_creases = [round(frame / 24.0, 3)] * 4

            # Check crease values
            blender_crease_data = [round(d.value, 3) for d in blender_mesh1_eval.data.attributes["crease_edge"].data]
            self.assertEqual(
                blender_crease_data,
                expected_edge_creases,
                f"Frame {frame}: {blender_mesh1_eval.name} crease values do not match")

            blender_crease_data = [round(d.value, 3) for d in blender_mesh2_eval.data.attributes["crease_vert"].data]
            self.assertEqual(
                blender_crease_data,
                expected_vert_creases,
                f"Frame {frame}: {blender_mesh2_eval.name} crease values do not match")

    def do_export_import_test(self, *, flatten: bool):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "camera_transforms.blend"))

        abc_path = self.tempdir / "camera_transforms.abc"
        self.assertIn('FINISHED', bpy.ops.wm.alembic_export(
            filepath=str(abc_path),
            flatten=flatten,
        ))

        # Re-import what we just exported into an empty file.
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "empty.blend"))
        self.assertIn('FINISHED', bpy.ops.wm.alembic_import(filepath=str(abc_path)))

        # Test that the import was ok.
        bpy.context.scene.frame_set(1)
        self.loc_rot_scale('CAM_Unit_Transform', (0, 0, 0), (0, 0, 0))

        self.loc_rot_scale('CAM_Look_+Y', (2, 0, 0), (90, 0, 0))
        self.loc_rot_scale('CAM_Static_Child_Left', (2 - 0.15, 0, 0), (90, 0, 0))
        self.loc_rot_scale('CAM_Static_Child_Right', (2 + 0.15, 0, 0), (90, 0, 0))
        self.loc_rot_scale('Static_Child', (2, 0, 1), (90, 0, 0))

        self.loc_rot_scale('CAM_Animated', (4, 0, 0), (90, 0, 0))
        self.loc_rot_scale('CAM_Animated_Child_Left', (4 - 0.15, 0, 0), (90, 0, 0))
        self.loc_rot_scale('CAM_Animated_Child_Right', (4 + 0.15, 0, 0), (90, 0, 0))
        self.loc_rot_scale('Animated_Child', (4, 0, 1), (90, 0, 0))

        bpy.context.scene.frame_set(10)

        self.loc_rot_scale('CAM_Animated', (4, 1, 2), (90, 0, 25))
        self.loc_rot_scale('CAM_Animated_Child_Left', (3.864053, 0.936607, 2), (90, 0, 25))
        self.loc_rot_scale('CAM_Animated_Child_Right', (4.135946, 1.063392, 2), (90, 0, 25))
        self.loc_rot_scale('Animated_Child', (4, 1, 3), (90, -45, 25))

    def loc_rot_scale(self, name: str, expect_loc, expect_rot_deg):
        """Assert world loc/rot/scale is OK."""

        objects = bpy.context.scene.collection.objects
        depsgraph = bpy.context.evaluated_depsgraph_get()
        ob_eval = objects[name].evaluated_get(depsgraph)

        actual_loc = ob_eval.matrix_world.to_translation()
        actual_rot = ob_eval.matrix_world.to_euler('XYZ')
        actual_scale = ob_eval.matrix_world.to_scale()

        # Precision of the 'almost equal' comparisons.
        delta_loc = delta_scale = 1e-6
        delta_rot = math.degrees(1e-6)

        self.assertAlmostEqual(expect_loc[0], actual_loc.x, delta=delta_loc)
        self.assertAlmostEqual(expect_loc[1], actual_loc.y, delta=delta_loc)
        self.assertAlmostEqual(expect_loc[2], actual_loc.z, delta=delta_loc)

        self.assertAlmostEqual(expect_rot_deg[0], math.degrees(actual_rot.x), delta=delta_rot)
        self.assertAlmostEqual(expect_rot_deg[1], math.degrees(actual_rot.y), delta=delta_rot)
        self.assertAlmostEqual(expect_rot_deg[2], math.degrees(actual_rot.z), delta=delta_rot)

        # This test doesn't use scale.
        self.assertAlmostEqual(1, actual_scale.x, delta=delta_scale)
        self.assertAlmostEqual(1, actual_scale.y, delta=delta_scale)
        self.assertAlmostEqual(1, actual_scale.z, delta=delta_scale)


class OverrideLayersTest(AbstractAlembicTest):
    def test_import_layer(self):
        fname = 'cube-base-file.abc'
        fname_layer = 'cube-hi-res.abc'
        abc = self.testdir / fname
        abc_layer = self.testdir / fname_layer

        # We need a cache reader to ensure that the data will be updated after adding a layer.
        res = bpy.ops.wm.alembic_import(filepath=str(abc), as_background_job=False, always_add_cache_reader=True)
        self.assertEqual({'FINISHED'}, res)

        # Check that the file loaded ok.
        cube = bpy.context.active_object
        depsgraph = bpy.context.evaluated_depsgraph_get()
        scene = bpy.context.scene
        cube_eval = cube.evaluated_get(depsgraph)
        mesh = cube_eval.to_mesh()

        # The base file should be a default cube.
        self.assertEqual(len(mesh.vertices), 8)
        self.assertEqual(len(mesh.edges), 12)
        self.assertEqual(len(mesh.polygons), 6)

        # Add a layer.
        cache_file = bpy.data.cache_files[fname]
        self.assertEqual(len(cache_file.layers), 0)

        layer = cache_file.layers.new(filepath=str(abc_layer))
        self.assertEqual(len(cache_file.layers), 1)
        self.assertIsNotNone(layer)

        # The layer added a higher res version of the mesh.
        depsgraph = bpy.context.evaluated_depsgraph_get()
        cube_eval = cube.evaluated_get(depsgraph)
        mesh = cube_eval.to_mesh()
        self.assertEqual(len(mesh.vertices), 26)
        self.assertEqual(len(mesh.edges), 48)
        self.assertEqual(len(mesh.polygons), 24)

        # Remove the layer.
        cache_file.layers.remove(layer)
        self.assertEqual(len(cache_file.layers), 0)

        # We should have reverted to the default cube.
        depsgraph = bpy.context.evaluated_depsgraph_get()
        cube_eval = cube.evaluated_get(depsgraph)
        mesh = cube_eval.to_mesh()
        self.assertEqual(len(mesh.vertices), 8)
        self.assertEqual(len(mesh.edges), 12)
        self.assertEqual(len(mesh.polygons), 6)


class AlembicImportComparisonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir
        cls.output_dir = args.outdir

    def test_import_alembic(self):
        comparisondir = self.testdir.joinpath("compare")
        input_files = sorted(pathlib.Path(comparisondir).glob("*.abc"))
        self.passed_tests = []
        self.failed_tests = []
        self.updated_tests = []

        from modules import io_report
        report = io_report.Report("Alembic Import", self.output_dir, comparisondir, comparisondir.joinpath("reference"))
        io_report.Report.context_lines = 8

        for input_file in input_files:
            input_file_path = pathlib.Path(input_file)

            io_report.Report.side_to_print_single_line = 5
            io_report.Report.side_to_print_multi_line = 3

            with self.subTest(input_file_path.stem):
                bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
                ok = report.import_and_check(
                    input_file, lambda filepath, params: bpy.ops.wm.alembic_import(
                        filepath=str(input_file), **params))
                if not ok:
                    self.fail(f"{input_file.stem} import result does not match expectations")

        report.finish("io_alembic_import")


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    parser.add_argument('--outdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
