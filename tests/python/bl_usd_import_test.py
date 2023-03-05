# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys
import unittest

import bpy

args = None


class AbstractUSDTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        'Test dir {0} should exist'.format(self.testdir))

        # Make sure we always start with a known-empty file.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))


class USDImportTest(AbstractUSDTest):

    def test_import_operator(self):
        """Test running the import operator on valid and invalid files."""

        infile = str(self.testdir / "usd_mesh_polygon_types.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        infile = str(self.testdir / "this_file_doesn't_exist.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'CANCELLED'}, res, "Was somehow able to import a non-existent USD file!")

    def test_import_prim_hierarchy(self):
        """Test importing a simple object hierarchy from a USDA file."""

        infile = str(self.testdir / "prim-hierarchy.usda")

        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        objects = bpy.context.scene.collection.objects
        self.assertEqual(5, len(objects), f"Test scene {infile} should have five objects; found {len(objects)}")

        # Test the hierarchy.
        self.assertIsNone(objects['World'].parent, "/World should not be parented.")
        self.assertEqual(objects['World'], objects['Plane'].parent, "Plane should be child of /World")
        self.assertEqual(objects['World'], objects['Plane_001'].parent, "Plane_001 should be a child of /World")
        self.assertEqual(objects['World'], objects['Empty'].parent, "Empty should be a child of /World")
        self.assertEqual(objects['Empty'], objects['Plane_002'].parent, "Plane_002 should be a child of /World")

    def test_import_mesh_topology(self):
        """Test importing meshes with different polygon types."""

        infile = str(self.testdir / "usd_mesh_polygon_types.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        objects = bpy.context.scene.collection.objects
        self.assertEqual(5, len(objects), f"Test scene {infile} should have five objects; found {len(objects)}")

        # Test topology counts.
        self.assertIn("m_degenerate", objects, "Scene does not contain object m_degenerate")
        mesh = objects["m_degenerate"].data
        self.assertEqual(len(mesh.polygons), 2)
        self.assertEqual(len(mesh.edges), 7)
        self.assertEqual(len(mesh.vertices), 6)

        self.assertIn("m_triangles", objects, "Scene does not contain object m_triangles")
        mesh = objects["m_triangles"].data
        self.assertEqual(len(mesh.polygons), 2)
        self.assertEqual(len(mesh.edges), 5)
        self.assertEqual(len(mesh.vertices), 4)
        self.assertEqual(len(mesh.polygons[0].vertices), 3)

        self.assertIn("m_quad", objects, "Scene does not contain object m_quad")
        mesh = objects["m_quad"].data
        self.assertEqual(len(mesh.polygons), 1)
        self.assertEqual(len(mesh.edges), 4)
        self.assertEqual(len(mesh.vertices), 4)
        self.assertEqual(len(mesh.polygons[0].vertices), 4)

        self.assertIn("m_ngon_concave", objects, "Scene does not contain object m_ngon_concave")
        mesh = objects["m_ngon_concave"].data
        self.assertEqual(len(mesh.polygons), 1)
        self.assertEqual(len(mesh.edges), 5)
        self.assertEqual(len(mesh.vertices), 5)
        self.assertEqual(len(mesh.polygons[0].vertices), 5)

        self.assertIn("m_ngon_convex", objects, "Scene does not contain object m_ngon_convex")
        mesh = objects["m_ngon_convex"].data
        self.assertEqual(len(mesh.polygons), 1)
        self.assertEqual(len(mesh.edges), 5)
        self.assertEqual(len(mesh.vertices), 5)
        self.assertEqual(len(mesh.polygons[0].vertices), 5)

    def test_import_mesh_uv_maps(self):
        """Test importing meshes with udim UVs and multiple UV sets."""

        infile = str(self.testdir / "usd_mesh_udim.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        objects = bpy.context.scene.collection.objects
        if "preview" in bpy.data.objects:
            bpy.data.objects.remove(bpy.data.objects["preview"])
        self.assertEqual(1, len(objects), f"File {infile} should contain one object, found {len(objects)}")

        mesh = bpy.data.objects["uvmap_plane"].data
        self.assertEqual(len(mesh.uv_layers), 2,
                         f"Object uvmap_plane should have two uv layers, found {len(mesh.uv_layers)}")

        expected_layer_names = {"udim_map", "uvmap"}
        imported_layer_names = set(mesh.uv_layers.keys())
        self.assertEqual(
            expected_layer_names,
            imported_layer_names,
            f"Expected layer names ({expected_layer_names}) not found on uvmap_plane.")

        def get_coords(data):
            coords = [x.uv for x in uvmap]
            return coords

        def uv_min_max(data):
            coords = get_coords(data)
            uv_min_x = min([uv[0] for uv in coords])
            uv_max_x = max([uv[0] for uv in coords])
            uv_min_y = min([uv[1] for uv in coords])
            uv_max_y = max([uv[1] for uv in coords])
            return uv_min_x, uv_max_x, uv_min_y, uv_max_y

        # Quick tests for point range.
        uvmap = mesh.uv_layers["uvmap"].data
        self.assertEqual(len(uvmap), 128)
        min_x, max_x, min_y, max_y = uv_min_max(uvmap)
        self.assertGreaterEqual(min_x, 0.0)
        self.assertGreaterEqual(min_y, 0.0)
        self.assertLessEqual(max_x, 1.0)
        self.assertLessEqual(max_y, 1.0)

        uvmap = mesh.uv_layers["udim_map"].data
        self.assertEqual(len(uvmap), 128)
        min_x, max_x, min_y, max_y = uv_min_max(uvmap)
        self.assertGreaterEqual(min_x, 0.0)
        self.assertGreaterEqual(min_y, 0.0)
        self.assertLessEqual(max_x, 2.0)
        self.assertLessEqual(max_y, 1.0)

        # Make sure at least some points are in a udim tile.
        coords = get_coords(uvmap)
        coords = list(filter(lambda x: x[0] > 1.0, coords))
        self.assertGreater(len(coords), 16)

    def test_import_camera_properties(self):
        """Test importing camera to ensure properties set correctly."""

        # This file has metersPerUnit = 1
        infile = str(self.testdir / "usd_camera_test_1.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res)

        camera_object = bpy.data.objects["Test_Camera"]
        test_cam = camera_object.data

        self.assertAlmostEqual(43.12, test_cam.lens, 2)
        self.assertAlmostEqual(24.89, test_cam.sensor_width, 2)
        self.assertAlmostEqual(14.00, test_cam.sensor_height, 2)
        self.assertAlmostEqual(12.34, test_cam.shift_x, 2)
        self.assertAlmostEqual(56.78, test_cam.shift_y, 2)

        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()

        # This file has metersPerUnit = 0.1
        infile = str(self.testdir / "usd_camera_test_2.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res)

        camera_object = bpy.data.objects["Test_Camera"]
        test_cam = camera_object.data

        self.assertAlmostEqual(4.312, test_cam.lens, 3)
        self.assertAlmostEqual(2.489, test_cam.sensor_width, 3)
        self.assertAlmostEqual(1.400, test_cam.sensor_height, 3)
        self.assertAlmostEqual(1.234, test_cam.shift_x, 3)
        self.assertAlmostEqual(5.678, test_cam.shift_y, 3)


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
    main()
