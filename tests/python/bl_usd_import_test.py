# SPDX-FileCopyrightText: 2021-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import math
import os
import pathlib
import sys
import tempfile
import unittest
from pxr import Ar, Gf, Sdf, Usd, UsdGeom, UsdShade

import bpy

sys.path.append(str(pathlib.Path(__file__).parent.absolute()))
from modules.colored_print import (print_message, use_message_colors)


args = None


class AbstractUSDTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir
        if os.environ.get("BLENDER_TEST_COLOR") is not None:
            use_message_colors()

    def setUp(self):
        self._tempdir = tempfile.TemporaryDirectory()
        self.tempdir = pathlib.Path(self._tempdir.name)

        self.assertTrue(self.testdir.exists(),
                        'Test dir {0} should exist'.format(self.testdir))
        self.assertTrue(self.tempdir.exists(),
                        'Temp dir {0} should exist'.format(self.tempdir))

        print_message(self._testMethodName, 'SUCCESS', 'RUN')

        # Make sure we always start with a known-empty file.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))

    def tearDown(self):
        self._tempdir.cleanup()

        result = self._outcome.result
        ok = all(test != self for test, _ in result.errors + result.failures)
        if not ok:
            print_message(self._testMethodName, 'FAILURE', 'FAILED')
        else:
            print_message(self._testMethodName, 'SUCCESS', 'PASSED')


class USDImportTest(AbstractUSDTest):
    # Utility function to round each component of a vector to a few digits. The "+ 0" is to
    # ensure that any negative zeros (-0.0) are converted to positive zeros (0.0).
    @staticmethod
    def round_vector(vector, digits=5):
        return [round(c, digits) + 0 for c in vector]

    def test_import_operator(self):
        """Test running the import operator on valid and invalid files."""

        infile = str(self.testdir / "usd_mesh_polygon_types.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        infile = str(self.testdir / "this_file_doesn't_exist.usda")
        # RPT_ERROR Reports from operators generate `RuntimeError` python exceptions.
        try:
            res = bpy.ops.wm.usd_import(filepath=infile)
            self.assertEqual({'CANCELLED'}, res, "Was somehow able to import a non-existent USD file!")
        except RuntimeError as e:
            self.assertTrue(e.args[0].startswith("Error: USD Import: unable to open stage to read"))

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

    def test_import_xform_and_mesh_merged_false(self):
        """Test importing a simple object hierarchy (xform and mesh) from a USDA file."""

        infile = str(self.testdir / "usd_mesh_polygon_types.usda")

        res = bpy.ops.wm.usd_import(filepath=infile, merge_parent_xform=False)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        objects = bpy.context.scene.collection.objects
        self.assertEqual(10, len(objects), f"Test scene {infile} should have ten objects; found {len(objects)}")

        # Test the hierarchy.
        self.assertEqual(
            objects['degenerate'],
            objects['m_degenerate'].parent,
            "m_degenerate should be child of /degenerate")
        self.assertEqual(
            objects['triangles'],
            objects['m_triangles'].parent,
            "m_triangles should be a child of /triangles")
        self.assertEqual(objects['quad'], objects['m_quad'].parent, "m_quad should be a child of /quad")
        self.assertEqual(objects['ngon_concave'], objects['m_ngon_concave'].parent,
                         "m_ngon_concave should be a child of /ngon_concave")

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
        self.assertEqual(len(mesh.polygons), 0)
        self.assertEqual(len(mesh.edges), 0)
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

    def test_import_mesh_topology_change(self):
        """Test importing meshes with changing topology over time."""

        infile = str(self.testdir / "usd_mesh_topology_change.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        # Check topology for all frames against expected vertex and face counts
        expected_face_verts = [
            (4, 4, 4),
            (3, 4, 5),
            (3, 3, 6),
            (4, 4, 4),
        ]
        for frame in range(1, 5):
            bpy.context.scene.frame_set(frame)
            depsgraph = bpy.context.evaluated_depsgraph_get()

            mesh = depsgraph.objects["TopoTest"].data

            expected = expected_face_verts[frame - 1]
            self.assertEqual(len(mesh.polygons), len(expected), f"Unexpected data for {frame=}")
            for face in range(0, 3):
                verts = mesh.polygons[face].vertices
                self.assertEqual(len(verts), expected[face], f"Unexpected data for {frame=} {face=}")

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

    def test_import_mesh_subd(self):
        """Test importing meshes with subdivision attributes."""

        # Use the existing mesh subd test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_mesh_subd.blend"))
        testfile = str(self.tempdir / "usd_mesh_subd.usda")
        bpy.ops.wm.usd_export(filepath=testfile, export_subdivision='BEST_MATCH', evaluation_mode="RENDER")
        res = bpy.ops.wm.usd_export(filepath=testfile, export_materials=True)
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile, import_subdivision=True)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # Validate crease attributes
        mesh = bpy.data.objects["crease_verts"].data
        blender_crease_data = [round(d.value, 5) for d in mesh.attributes["crease_vert"].data]
        self.assertEqual(blender_crease_data, [0.3, 0.0, 0.2, 0.1, 0.8, 0.7, 1.0, 0.9])

        mesh = bpy.data.objects["crease_edge"].data
        blender_crease_data = [round(d.value, 5) for d in mesh.attributes["crease_edge"].data]
        self.assertEqual(
            blender_crease_data,
            [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.9, 0.0, 0.0, 0.8, 0.0, 0.0, 0.7, 0.0,
             0.0, 0.6, 0.0, 0.0, 0.5, 0.0, 0.0, 0.4, 0.0, 0.0, 0.3, 0.0, 0.0, 0.2, 0.0, 0.0, 0.1, 0.0, 0.0,
             0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0])

        # Validate SubdivisionSurface modifier settings
        def check_mod(mesh_name, levels, render_levels, uv_smooth, boundary_smooth):
            mod = bpy.data.objects[mesh_name].modifiers[0]
            self.assertEqual(mod.levels, levels)
            self.assertEqual(mod.render_levels, render_levels)
            self.assertEqual(mod.uv_smooth, uv_smooth)
            self.assertEqual(mod.boundary_smooth, boundary_smooth)

        check_mod("mesh1", 1, 2, 'NONE', 'ALL')
        check_mod("mesh2", 1, 2, 'PRESERVE_CORNERS', 'ALL')
        check_mod("mesh3", 1, 2, 'PRESERVE_CORNERS_AND_JUNCTIONS', 'ALL')
        check_mod("mesh4", 1, 2, 'PRESERVE_CORNERS_JUNCTIONS_AND_CONCAVE', 'ALL')
        check_mod("mesh5", 1, 2, 'PRESERVE_BOUNDARIES', 'ALL')
        check_mod("mesh6", 1, 2, 'SMOOTH_ALL', 'ALL')
        check_mod("mesh7", 1, 2, 'PRESERVE_BOUNDARIES', 'PRESERVE_CORNERS')

    def test_import_mesh_subd_varying(self):
        """Test importing meshes with subdivision crease values varying over time."""

        testfile = str(self.testdir / "usd_mesh_subd_varying_test.usda")
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        stage = Usd.Stage.Open(testfile)

        #
        # Validate Mesh data
        #
        blender_mesh1 = bpy.data.objects["mesh_edge_crease"]
        blender_mesh2 = bpy.data.objects["mesh_vert_crease"]
        usd_mesh1 = UsdGeom.Mesh(stage.GetPrimAtPath("/root/mesh_edge_crease/mesh_edge_crease"))
        usd_mesh2 = UsdGeom.Mesh(stage.GetPrimAtPath("/root/mesh_vert_crease/mesh_vert_crease"))

        # A MeshSequenceCache modifier should be present on every imported object
        for blender_mesh in [blender_mesh1, blender_mesh2]:
            self.assertTrue(len(blender_mesh.modifiers) == 1 and blender_mesh.modifiers[0].type ==
                            'MESH_SEQUENCE_CACHE', f"{blender_mesh.name} has incorrect modifiers")

        # Conversion from USD to Blender convention
        def sharpness_to_crease(sharpness):
            return math.sqrt(sharpness * 0.1)

        # Compare Blender and USD data against each other for every frame
        for frame in range(1, 25):
            bpy.context.scene.frame_set(frame)
            depsgraph = bpy.context.evaluated_depsgraph_get()
            blender_mesh1_eval = bpy.data.objects["mesh_edge_crease"].evaluated_get(depsgraph)
            blender_mesh2_eval = bpy.data.objects["mesh_vert_crease"].evaluated_get(depsgraph)

            # Check crease values
            blender_crease_data = [round(d.value, 5) for d in blender_mesh1_eval.data.attributes["crease_edge"].data]
            usd_crease_data = [
                round(sharpness_to_crease(d), 5) for d in usd_mesh1.GetCreaseSharpnessesAttr().Get(frame)]
            self.assertEqual(
                blender_crease_data,
                usd_crease_data,
                f"Frame {frame}: {blender_mesh1_eval.name} crease values do not match")

            blender_crease_data = [round(d.value, 5) for d in blender_mesh2_eval.data.attributes["crease_vert"].data]
            usd_crease_data = [
                round(sharpness_to_crease(d), 5) for d in usd_mesh2.GetCornerSharpnessesAttr().Get(frame)]
            self.assertEqual(
                blender_crease_data,
                usd_crease_data,
                f"Frame {frame}: {blender_mesh2_eval.name} crease values do not match")

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
        self.assertAlmostEqual(2.281, test_cam.shift_x, 2)
        self.assertAlmostEqual(0.496, test_cam.shift_y, 2)

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
        self.assertAlmostEqual(2.281, test_cam.shift_x, 3)
        self.assertAlmostEqual(0.496, test_cam.shift_y, 3)

    def assert_all_nodes_present(self, mat, node_list):
        nodes = mat.node_tree.nodes
        self.assertEqual(len(nodes), len(node_list))
        for node in node_list:
            self.assertTrue(nodes.find(node) >= 0, f"Could not find node '{node}' in material '{mat.name}'")

    def test_import_materials1(self):
        """Validate UsdPreviewSurface shader graphs."""

        # Use the existing materials test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_export.blend"))
        testfile = str(self.tempdir / "usd_materials_export.usda")
        res = bpy.ops.wm.usd_export(filepath=str(testfile), export_materials=True)
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # Most shader graph validation should occur through the Hydra render test suite. Here we
        # will only check some high-level criteria for each expected node graph.

        mat = bpy.data.materials["Transforms"]
        self.assert_all_nodes_present(mat, ["Principled BSDF", "Image Texture", "UV Map", "Mapping", "Material Output"])
        node = mat.node_tree.nodes["Mapping"]
        self.assertEqual(self.round_vector(node.inputs[1].default_value), [0.75, 0.75, 0])
        self.assertEqual(self.round_vector(node.inputs[2].default_value), [0, 0, 3.14159])
        self.assertEqual(self.round_vector(node.inputs[3].default_value), [0.5, 0.5, 1])

        mat = bpy.data.materials["NormalMap"]
        self.assert_all_nodes_present(
            mat, ["Principled BSDF", "Image Texture", "UV Map", "Normal Map", "Material Output"])

        mat = bpy.data.materials["NormalMap_Scale_Bias"]
        self.assert_all_nodes_present(mat, ["Principled BSDF", "Image Texture", "UV Map",
                                            "Normal Map", "Vector Math", "Vector Math.001", "Material Output"])
        node = mat.node_tree.nodes["Vector Math"]
        self.assertEqual(self.round_vector(node.inputs[1].default_value), [2, -2, 2])
        self.assertEqual(self.round_vector(node.inputs[2].default_value), [-1, 1, -1])

    def test_import_materials2(self):
        """Validate UsdPreviewSurface shader graphs."""

        # Use the existing materials test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_channels.blend"))
        testfile = str(self.tempdir / "usd_materials_channels.usda")
        res = bpy.ops.wm.usd_export(filepath=str(testfile), export_materials=True)
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # Most shader graph validation should occur through the Hydra render test suite. Here we
        # will only check some high-level criteria for each expected node graph.

        mat = bpy.data.materials["Opaque"]
        self.assert_all_nodes_present(mat, ["Principled BSDF", "Image Texture", "UV Map", "Material Output"])

        mat = bpy.data.materials["Alpha"]
        self.assert_all_nodes_present(mat, ["Principled BSDF", "Image Texture", "UV Map", "Material Output"])

        mat = bpy.data.materials["AlphaClip_LessThan"]
        self.assert_all_nodes_present(
            mat, ["Principled BSDF", "Image Texture", "UV Map", "Math", "Math.001", "Material Output"])
        node = [n for n in mat.node_tree.nodes if n.type == 'MATH' and n.operation == "LESS_THAN"][0]
        self.assertAlmostEqual(node.inputs[1].default_value, 0.8, 3)

        mat = bpy.data.materials["AlphaClip_Round"]
        self.assert_all_nodes_present(
            mat, ["Principled BSDF", "Image Texture", "UV Map", "Math", "Math.001", "Material Output"])
        node = [n for n in mat.node_tree.nodes if n.type == 'MATH' and n.operation == "LESS_THAN"][0]
        self.assertAlmostEqual(node.inputs[1].default_value, 0.5, 3)

        mat = bpy.data.materials["Channel"]
        self.assert_all_nodes_present(
            mat, ["Principled BSDF", "Image Texture", "Separate Color", "UV Map", "Material Output"])

        mat = bpy.data.materials["ChannelClip_LessThan"]
        self.assert_all_nodes_present(
            mat,
            ["Principled BSDF", "Image Texture", "Separate Color", "UV Map", "Math", "Math.001", "Material Output"])
        node = [n for n in mat.node_tree.nodes if n.type == 'MATH' and n.operation == "LESS_THAN"][0]
        self.assertAlmostEqual(node.inputs[1].default_value, 0.2, 3)

        mat = bpy.data.materials["ChannelClip_Round"]
        self.assert_all_nodes_present(
            mat,
            ["Principled BSDF", "Image Texture", "Separate Color", "UV Map", "Math", "Math.001", "Material Output"])
        node = [n for n in mat.node_tree.nodes if n.type == 'MATH' and n.operation == "LESS_THAN"][0]
        self.assertAlmostEqual(node.inputs[1].default_value, 0.5, 3)

    def test_import_material_subsets(self):
        """Validate multiple materials assigned to the same mesh work correctly."""

        # Use the existing materials test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_multi.blend"))
        # Ensure the simulation zone data is baked for all relevant frames...
        for frame in range(1, 5):
            bpy.context.scene.frame_set(frame)
        bpy.context.scene.frame_set(1)

        testfile = str(self.tempdir / "usd_materials_multi.usda")
        res = bpy.ops.wm.usd_export(filepath=testfile, export_animation=True, evaluation_mode="RENDER")
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # The static mesh should have 4 materials each assigned to 4 faces (16 faces total)
        static_mesh = bpy.data.objects["static_mesh"].data
        material_index_attr = static_mesh.attributes["material_index"]
        self.assertEqual(len(static_mesh.materials), 4)
        self.assertEqual(len(static_mesh.polygons), 16)
        self.assertEqual(len(material_index_attr.data), 16)

        for mat_index in range(0, 4):
            face_indices = [i for i, d in enumerate(material_index_attr.data) if d.value == mat_index]
            self.assertEqual(len(face_indices), 4, f"Incorrect number of faces with material index {mat_index}")

    def test_import_material_displacement(self):
        """Validate correct import of Displacement information for the UsdPreviewSurface"""

        # Use the existing materials test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_displace.blend"))
        testfile = str(self.tempdir / "temp_material_displace.usda")
        res = bpy.ops.wm.usd_export(filepath=str(testfile), export_materials=True)
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # Most shader graph validation should occur through the Hydra render test suite. Here we
        # will only check some high-level criteria for each expected node graph.

        def assert_displacement(mat, height, midlevel, scale):
            nodes = mat.node_tree.nodes
            node_displace_index = nodes.find("Displacement")
            self.assertTrue(node_displace_index >= 0)

            node_displace = nodes[node_displace_index]
            if height is not None:
                self.assertAlmostEqual(node_displace.inputs[0].default_value, height)
            else:
                self.assertEqual(len(node_displace.inputs[0].links), 1)
            self.assertAlmostEqual(node_displace.inputs[1].default_value, midlevel)
            self.assertAlmostEqual(node_displace.inputs[2].default_value, scale)

        mat = bpy.data.materials["constant"]
        assert_displacement(mat, 0.95, 0.5, 1.0)

        mat = bpy.data.materials["mid_1_0"]
        assert_displacement(mat, None, 1.0, 1.0)
        mat = bpy.data.materials["mid_0_5"]
        assert_displacement(mat, None, 0.5, 1.0)
        mat = bpy.data.materials["mid_0_0"]
        assert_displacement(mat, None, 0.0, 1.0)

        mat = bpy.data.materials["mid_1_0_scale_0_3"]
        assert_displacement(mat, None, 1.0, 0.3)
        mat = bpy.data.materials["mid_0_5_scale_0_3"]
        assert_displacement(mat, None, 0.5, 0.3)
        mat = bpy.data.materials["mid_0_0_scale_0_3"]
        assert_displacement(mat, None, 0.0, 0.3)

    def test_import_material_attributes(self):
        """Validate correct import of Attribute information from UsdPrimvarReaders"""

        # Use the existing materials test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_attributes.blend"))
        testfile = str(self.tempdir / "usd_materials_attributes.usda")
        res = bpy.ops.wm.usd_export(filepath=str(testfile), export_materials=True)
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # Most shader graph validation should occur through the Hydra render test suite. Here we
        # will only check some high-level criteria for each expected node graph.

        def assert_attribute(mat, attribute_name, from_socket, to_socket):
            nodes = [n for n in mat.node_tree.nodes if n.type == 'ATTRIBUTE' and n.attribute_name == attribute_name]
            self.assertTrue(len(nodes) == 1)
            outputs = [o for o in nodes[0].outputs if o.identifier == from_socket]
            self.assertTrue(len(outputs) == 1)
            self.assertTrue(len(outputs[0].links) == 1)
            link = outputs[0].links[0]
            self.assertEqual(link.from_socket.identifier, from_socket)
            self.assertEqual(link.to_socket.identifier, to_socket)

        mat = bpy.data.materials["Material"]
        self.assert_all_nodes_present(
            mat, ["Principled BSDF", "Attribute", "Attribute.001", "Attribute.002", "Material Output"])

        assert_attribute(mat, "displayColor", "Color", "Base Color")
        assert_attribute(mat, "f_vec", "Vector", "Normal")
        assert_attribute(mat, "f_float", "Fac", "Roughness")

    def test_import_material_node_graph(self):
        """Verify we can follow connections through NodeGraph defs."""

        testfile = str(self.testdir / "usd_materials_node_graph.usda")
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res)

        # If NodeGraph traversal is missing or broken, the Image Texture and UV Map nodes will be missing
        mat = bpy.data.materials["Material"]
        self.assert_all_nodes_present(mat, ["Principled BSDF", "Image Texture", "UV Map", "Material Output"])

    def check_mat_data(self, mat_name, expected_users, expected_color, ob_names):
        if expected_users < 0:
            self.assertEqual(bpy.data.materials.find(mat_name), -1)
            return

        mat = bpy.data.materials[mat_name]
        self.assertEqual(mat.users, expected_users)
        self.assertEqual(mat.diffuse_color[0:3], expected_color)
        for ob_name in ob_names:
            mat_slot = bpy.data.objects[ob_name].material_slots[0]
            self.assertEqual(mat_slot.name, mat_name, f"Object {ob_name} has incorrect material")

    def test_import_material_collisions(self):
        """Validate that material name collisions are properly handled"""

        # Variation with multiple objects referencing the same common materials
        testfile = str(self.testdir / "usd_materials_collision.usda")

        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        bpy.data.materials.new(name="Material").diffuse_color = (1, 0, 1, 1)
        res = bpy.ops.wm.usd_import(filepath=testfile, mtl_name_collision_mode='MAKE_UNIQUE')
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        self.assertEqual(len(bpy.data.materials), 3)
        self.check_mat_data("Material", 0, (1, 0, 1), [])
        self.check_mat_data("Material.001", 3, (1, 0, 0), ["o1", "o3", "o5"])
        self.check_mat_data("Material_001", 3, (0, 0, 1), ["o2", "o4", "o6"])

        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        bpy.data.materials.new(name="Material").diffuse_color = (1, 0, 1, 1)
        res = bpy.ops.wm.usd_import(filepath=testfile, mtl_name_collision_mode='REFERENCE_EXISTING')
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        self.assertEqual(len(bpy.data.materials), 2)
        self.check_mat_data("Material", 3, (1, 0, 1), ["o1", "o3", "o5"])
        self.check_mat_data("Material.001", -1, (), [])
        self.check_mat_data("Material_001", 3, (0, 0, 1), ["o2", "o4", "o6"])

    def test_import_material_collisions2(self):
        """Validate that material name collisions are properly handled"""

        # Variation with multiple materials with the same name but under different paths
        testfile = str(self.testdir / "usd_materials_collision2.usda")

        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile, mtl_name_collision_mode='MAKE_UNIQUE')
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # Due to out of order reading, we know that there should be 3 materials, but we don't know
        # which material(name) ended up on each object. The viewport color of the material should
        # match what we expect though
        self.assertEqual(len(bpy.data.materials), 3)
        o1_mat_name = bpy.data.objects["o1"].material_slots[0].name
        o2_mat_name = bpy.data.objects["o2"].material_slots[0].name
        o3_mat_name = bpy.data.objects["o3"].material_slots[0].name
        self.check_mat_data(o1_mat_name, 1, (1, 0, 0), ["o1"])
        self.check_mat_data(o2_mat_name, 1, (0, 1, 0), ["o2"])
        self.check_mat_data(o3_mat_name, 1, (0, 0, 1), ["o3"])

        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile, mtl_name_collision_mode='REFERENCE_EXISTING')
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # Due to out of order reading, we know that there should be 1 material, but we don't know
        # which color ended up "winning" during the Import process
        self.assertEqual(len(bpy.data.materials), 1)
        expected_color = bpy.data.materials["MaterialA"].diffuse_color[0:3]
        self.check_mat_data("MaterialA", 3, expected_color, ["o1", "o2", "o3"])

    def test_import_shader_varname_with_connection(self):
        """Test importing USD shader where uv primvar is a connection"""

        varname = "testmap"
        texfile = str(self.testdir / "textures/test_grid_1001.png")

        # Create the test USD file.
        temp_usd_file = str(self.tempdir / "usd_varname_test.usda")
        stage = Usd.Stage.CreateNew(temp_usd_file)
        mesh1 = stage.DefinePrim("/mesh1", "Mesh")
        mesh2 = stage.DefinePrim("/mesh2", "Mesh")

        # Create two USD preview surface shaders in two materials.
        m1 = UsdShade.Material.Define(stage, "/mat1")
        s1 = UsdShade.Shader.Define(stage, "/mat1/previewshader")
        s1.CreateIdAttr("UsdPreviewSurface")
        m1.CreateSurfaceOutput().ConnectToSource(s1.ConnectableAPI(), "surface")
        t1 = UsdShade.Shader.Define(stage, "/mat1/diffuseTexture")
        t1.CreateIdAttr("UsdUVTexture")
        t1.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(texfile)
        t1.CreateOutput("rgb", Sdf.ValueTypeNames.Float3)
        s1.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(t1.ConnectableAPI(), "rgb")
        t2 = UsdShade.Shader.Define(stage, "/mat1/roughnessTexture")
        t2.CreateIdAttr("UsdUVTexture")
        t2.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(texfile)
        t2.CreateOutput("rgb", Sdf.ValueTypeNames.Float3)
        s1.CreateInput("roughness", Sdf.ValueTypeNames.Color3f).ConnectToSource(t2.ConnectableAPI(), "rgb")

        m2 = UsdShade.Material.Define(stage, "/mat2")
        s2 = UsdShade.Shader.Define(stage, "/mat2/previewshader")
        s2.CreateIdAttr("UsdPreviewSurface")
        m2.CreateSurfaceOutput().ConnectToSource(s2.ConnectableAPI(), "surface")
        t3 = UsdShade.Shader.Define(stage, "/mat2/diffuseTexture")
        t3.CreateIdAttr("UsdUVTexture")
        t3.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(texfile)
        t3.CreateOutput("rgb", Sdf.ValueTypeNames.Float3)
        s2.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(t3.ConnectableAPI(), "rgb")
        t4 = UsdShade.Shader.Define(stage, "/mat2/roughnessTexture")
        t4.CreateIdAttr("UsdUVTexture")
        t4.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(texfile)
        t4.CreateOutput("rgb", Sdf.ValueTypeNames.Float3)
        s2.CreateInput("roughness", Sdf.ValueTypeNames.Color3f).ConnectToSource(t4.ConnectableAPI(), "rgb")

        # Bind mat1 to mesh1, mat2 to mesh2.
        bindingAPI = UsdShade.MaterialBindingAPI.Apply(mesh1)
        bindingAPI.Bind(m1)
        bindingAPI = UsdShade.MaterialBindingAPI.Apply(mesh2)
        bindingAPI.Bind(m2)

        # Create varname defined as a token.
        s3 = UsdShade.Shader.Define(stage, "/mat1/primvar_reader1")
        s3.CreateIdAttr('UsdPrimvarReader_float2')
        s3input = s3.CreateInput("varname", Sdf.ValueTypeNames.Token)
        s3input.Set(varname)
        t1.CreateInput("st", Sdf.ValueTypeNames.TexCoord2f).ConnectToSource(s3.ConnectableAPI(), "result")

        # Create varname defined as a connection to a token.
        varname1 = m1.CreateInput("varname", Sdf.ValueTypeNames.Token)
        varname1.Set(varname)
        s4 = UsdShade.Shader.Define(stage, "/mat1/primvar_reader2")
        s4.CreateIdAttr('UsdPrimvarReader_float2')
        s4input = s4.CreateInput("varname", Sdf.ValueTypeNames.Token)
        UsdShade.ConnectableAPI.ConnectToSource(s4input, varname1)
        t2.CreateInput("st", Sdf.ValueTypeNames.TexCoord2f).ConnectToSource(s4.ConnectableAPI(), "result")

        # Create varname defined as a string.
        s5 = UsdShade.Shader.Define(stage, "/mat2/primvar_reader1")
        s5.CreateIdAttr('UsdPrimvarReader_float2')
        s5input = s5.CreateInput("varname", Sdf.ValueTypeNames.String)
        s5input.Set(varname)
        t3.CreateInput("st", Sdf.ValueTypeNames.TexCoord2f).ConnectToSource(s5.ConnectableAPI(), "result")

        # Create varname defined as a connection to a string.
        varname2 = m2.CreateInput("varname", Sdf.ValueTypeNames.String)
        varname2.Set(varname)
        s6 = UsdShade.Shader.Define(stage, "/mat2/primvar_reader2")
        s6.CreateIdAttr('UsdPrimvarReader_float2')
        s6input = s6.CreateInput("varname", Sdf.ValueTypeNames.String)
        UsdShade.ConnectableAPI.ConnectToSource(s6input, varname2)
        t4.CreateInput("st", Sdf.ValueTypeNames.TexCoord2f).ConnectToSource(s6.ConnectableAPI(), "result")

        stage.Save()

        # Now import the USD file.
        res = bpy.ops.wm.usd_import(filepath=temp_usd_file, import_all_materials=True)
        self.assertEqual({'FINISHED'}, res)

        # Ensure that we find the correct varname for all four primvar readers.
        num_uvmaps_found = 0
        mats_to_test = []
        mats_to_test.append(bpy.data.materials["mat1"])
        mats_to_test.append(bpy.data.materials["mat2"])
        for mat in mats_to_test:
            self.assertIsNotNone(mat.node_tree, "Material node tree is empty")
            for node in mat.node_tree.nodes:
                if node.type == "UVMAP":
                    self.assertEqual(varname, node.uv_map, "Unexpected value for varname")
                    num_uvmaps_found += 1

        self.assertEqual(4, num_uvmaps_found, "One or more test materials failed to import")

    def test_import_animation(self):
        """Test importing objects with xform, armature, and USD blend shape animations."""

        # Use the existing animation test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_anim_test.blend"))
        testfile = str(self.tempdir / "usd_anim_test.usda")
        res = bpy.ops.wm.usd_export(
            filepath=testfile,
            export_animation=True,
            evaluation_mode="RENDER",
        )
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # Validate some simple aspects of the animated objects which prove that they're animating.
        ob_xform = bpy.data.objects["cube_anim_xform"]
        ob_xform_child = bpy.data.objects["cube_anim_child_mesh"]
        ob_shapekeys = bpy.data.objects["cube_anim_keys"]
        ob_arm = bpy.data.objects["column_anim_armature"]
        ob_arm2_side_a = bpy.data.objects["side_a"]
        ob_arm2_side_b = bpy.data.objects["side_b"]
        self.assertEqual(bpy.data.objects["Armature"].animation_data.action.name, "ArmatureAction_001")

        bpy.context.scene.frame_set(1)
        self.assertEqual(len(ob_xform.constraints), 1)
        self.assertEqual(len(ob_xform_child.constraints), 1)
        self.assertEqual(self.round_vector(ob_xform.matrix_world.translation), [0.0, -2.0, 0.0])
        self.assertEqual(self.round_vector(ob_xform_child.matrix_world.translation), [0.0, -2.0, 1.0])
        self.assertEqual(self.round_vector(ob_xform_child.matrix_world.to_euler('XYZ')), [0.0, 0.0, 0.0])
        self.assertEqual(self.round_vector(ob_shapekeys.dimensions), [1.0, 1.0, 1.0])
        self.assertEqual(self.round_vector(ob_arm.dimensions), [0.4, 0.4, 3.0])
        self.assertEqual(self.round_vector(ob_arm2_side_a.dimensions), [0.5, 0.0, 0.5])
        self.assertEqual(self.round_vector(ob_arm2_side_b.dimensions), [0.5, 0.0, 0.5])
        self.assertAlmostEqual(ob_arm2_side_a.matrix_world.to_euler('XYZ').z, 0, 5)
        self.assertAlmostEqual(ob_arm2_side_b.matrix_world.to_euler('XYZ').z, 0, 5)

        bpy.context.scene.frame_set(5)
        self.assertEqual(len(ob_xform.constraints), 1)
        self.assertEqual(len(ob_xform_child.constraints), 1)
        self.assertEqual(self.round_vector(ob_xform.matrix_world.translation), [3.0, -2.0, 0.0])
        self.assertEqual(self.round_vector(ob_xform_child.matrix_world.translation), [3.0, -2.0, 1.0])
        self.assertEqual(self.round_vector(ob_xform_child.matrix_world.to_euler('XYZ')), [0.0, 1.5708, 0.0])
        self.assertEqual(self.round_vector(ob_shapekeys.dimensions), [0.1, 0.1, 0.1])
        self.assertEqual(self.round_vector(ob_arm.dimensions), [1.65545, 0.4, 2.38953])
        self.assertEqual(self.round_vector(ob_arm2_side_a.dimensions), [0.25, 0.0, 0.25])
        self.assertEqual(self.round_vector(ob_arm2_side_b.dimensions), [1.0, 0.0, 1.0])
        self.assertAlmostEqual(ob_arm2_side_a.matrix_world.to_euler('XYZ').z, 1.5708, 5)
        self.assertAlmostEqual(ob_arm2_side_b.matrix_world.to_euler('XYZ').z, 1.5708, 5)

    def test_import_volumes(self):
        """Validate volume import."""

        # Use the existing volume test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_volumes.blend"))
        # Ensure the simulation zone data is baked for all relevant frames...
        for frame in range(4, 15):
            bpy.context.scene.frame_set(frame)
        bpy.context.scene.frame_set(4)

        testfile = str(self.tempdir / "usd_volumes.usda")
        res = bpy.ops.wm.usd_export(filepath=testfile, export_animation=True, evaluation_mode="RENDER")
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # Validate that all volumes are properly configured.
        vol_displace = bpy.data.objects["vol_displace"]
        vol_filesequence = bpy.data.objects["vol_filesequence"]
        vol_mesh2vol = bpy.data.objects["vol_mesh2vol"]
        vol_sim = bpy.data.objects["Volume"]

        def check_sequence(ob, frames, start, offset):
            self.assertTrue(ob.data.is_sequence)
            self.assertEqual(ob.data.frame_duration, frames)
            self.assertEqual(ob.data.frame_start, start)
            self.assertEqual(ob.data.frame_offset, offset)

        check_sequence(vol_displace, 11, 4, 3)
        check_sequence(vol_filesequence, 6, 8, 13)
        check_sequence(vol_mesh2vol, 11, 4, 3)
        check_sequence(vol_sim, 11, 4, 3)

        # Validate that their object dimensions are changing by spot checking 2 interesting frames
        bpy.context.scene.frame_set(8)
        dim_displace = vol_displace.dimensions.copy()
        dim_filesequence = vol_filesequence.dimensions.copy()
        dim_mesh2vol = vol_mesh2vol.dimensions.copy()
        dim_sim = vol_sim.dimensions.copy()

        bpy.context.scene.frame_set(12)
        self.assertTrue(vol_displace.dimensions != dim_displace)
        self.assertTrue(vol_filesequence.dimensions != dim_filesequence)
        self.assertTrue(vol_mesh2vol.dimensions != dim_mesh2vol)
        self.assertTrue(vol_sim.dimensions != dim_sim)

    def test_import_usd_blend_shapes(self):
        """Test importing USD blend shapes with animated weights."""

        infile = str(self.testdir / "usd_blend_shape_test.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res)

        obj = bpy.data.objects["Plane"]

        obj.active_shape_key_index = 1

        key = obj.active_shape_key
        self.assertEqual(key.name, "Key_1", "Unexpected shape key name")

        # Verify the number of shape key points.
        self.assertEqual(len(key.data), 4, "Unexpected number of shape key point")

        # Verify shape key point coordinates

        # Reference point values.
        refs = ((-2.51, -1.92, 0.20), (0.86, -1.46, -0.1),
                (-1.33, 1.29, .84), (1.32, 2.20, -0.42))

        for i in range(4):
            co = key.data[i].co
            ref = refs[i]
            # Compare coordinates.
            for j in range(3):
                self.assertAlmostEqual(co[j], ref[j], 2)

        # Verify the shape key values.
        bpy.context.scene.frame_set(1)
        self.assertAlmostEqual(key.value, .002, 1)
        bpy.context.scene.frame_set(30)
        self.assertAlmostEqual(key.value, .900, 3)
        bpy.context.scene.frame_set(60)
        self.assertAlmostEqual(key.value, .100, 3)

    def test_import_usd_skel_joints(self):
        """Test importing USD animated skeleton joints."""

        infile = str(self.testdir / "arm.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res)

        # Verify armature was imported.
        arm_obj = bpy.data.objects["Skel"]
        self.assertEqual(arm_obj.type, "ARMATURE", "'Skel' object is not an armature")

        arm = arm_obj.data
        bones = arm.bones

        # Verify bone parenting.
        self.assertIsNone(bones['Shoulder'].parent, "Shoulder bone should not be parented")
        self.assertEqual(bones['Shoulder'], bones['Elbow'].parent, "Elbow bone should be child of Shoulder bone")
        self.assertEqual(bones['Elbow'], bones['Hand'].parent, "Hand bone should be child of Elbow bone")

        # Verify armature modifier was created on the mesh.
        mesh_obj = bpy.data.objects['Arm']
        # Get all the armature modifiers on the mesh.
        arm_mods = [m for m in mesh_obj.modifiers if m.type == "ARMATURE"]
        self.assertEqual(len(arm_mods), 1, "Didn't get expected armatrue modifier")
        self.assertEqual(arm_mods[0].object, arm_obj, "Armature modifier does not reference the imported armature")

        # Verify expected deform groups.
        # There are 4 points in each group.
        for i in range(4):
            self.assertAlmostEqual(mesh_obj.vertex_groups['Hand'].weight(
                i), 1.0, 2, "Unexpected weight for Hand deform vert")
            self.assertAlmostEqual(mesh_obj.vertex_groups['Shoulder'].weight(
                4 + i), 1.0, 2, "Unexpected weight for Shoulder deform vert")
            self.assertAlmostEqual(mesh_obj.vertex_groups['Elbow'].weight(
                8 + i), 1.0, 2, "Unexpected weight for Elbow deform vert")

        action = bpy.data.actions['Anim1']
        channelbag = action.layers[0].strips[0].channelbags[0]

        # Verify the Elbow joint rotation animation.
        curve_path = 'pose.bones["Elbow"].rotation_quaternion'

        # Quat W
        f = channelbag.fcurves.find(curve_path, index=0)
        self.assertIsNotNone(f, "Couldn't find Elbow rotation quaternion W curve")
        self.assertAlmostEqual(f.evaluate(0), 1.0, 2, "Unexpected value for rotation quaternion W curve at frame 0")
        self.assertAlmostEqual(f.evaluate(10), 0.707, 2, "Unexpected value for rotation quaternion W curve at frame 10")

        # Quat X
        f = channelbag.fcurves.find(curve_path, index=1)
        self.assertIsNotNone(f, "Couldn't find Elbow rotation quaternion X curve")
        self.assertAlmostEqual(f.evaluate(0), 0.0, 2, "Unexpected value for rotation quaternion X curve at frame 0")
        self.assertAlmostEqual(f.evaluate(10), 0.707, 2, "Unexpected value for rotation quaternion X curve at frame 10")

        # Quat Y
        f = channelbag.fcurves.find(curve_path, index=2)
        self.assertIsNotNone(f, "Couldn't find Elbow rotation quaternion Y curve")
        self.assertAlmostEqual(f.evaluate(0), 0.0, 2, "Unexpected value for rotation quaternion Y curve at frame 0")
        self.assertAlmostEqual(f.evaluate(10), 0.0, 2, "Unexpected value for rotation quaternion Y curve at frame 10")

        # Quat Z
        f = channelbag.fcurves.find(curve_path, index=3)
        self.assertIsNotNone(f, "Couldn't find Elbow rotation quaternion Z curve")
        self.assertAlmostEqual(f.evaluate(0), 0.0, 2, "Unexpected value for rotation quaternion Z curve at frame 0")
        self.assertAlmostEqual(f.evaluate(10), 0.0, 2, "Unexpected value for rotation quaternion Z curve at frame 10")

    def check_curve(self, blender_curve, usd_curve):
        curve_type_map = {"linear-bezier": 1, "linear-catmullRom": 1, "cubic-bezier": 2, "cubic-bspline": 3}
        cyclic_map = {"pinned": False, "nonperiodic": False, "periodic": True}

        # Check correct spline count.
        blender_spline_count = len(blender_curve.attributes["curve_type"].data)
        usd_spline_count = len(usd_curve.GetCurveVertexCountsAttr().Get())
        self.assertEqual(blender_spline_count, usd_spline_count)

        # Check correct type of curve. All splines should have the same type and periodicity.
        usd_curve_type_basis = usd_curve.GetTypeAttr().Get() + "-" + usd_curve.GetBasisAttr().Get()
        usd_cyclic = usd_curve.GetWrapAttr().Get()
        expected_curve_type = curve_type_map[usd_curve_type_basis]
        expected_cyclic = cyclic_map[usd_cyclic]

        for i in range(0, blender_spline_count):
            blender_curve_type = blender_curve.attributes["curve_type"].data[i].value
            blender_cyclic = False
            if "cyclic" in blender_curve.attributes:
                blender_cyclic = blender_curve.attributes["cyclic"].data[i].value

            self.assertEqual(blender_curve_type, expected_curve_type)
            self.assertEqual(blender_cyclic, expected_cyclic)

        # Check position data.
        usd_positions = usd_curve.GetPointsAttr().Get()
        blender_positions = blender_curve.attributes["position"].data

        point_count = 0
        if usd_curve_type_basis in ("linear-bezier", "linear-catmullRom"):
            point_count = len(usd_positions)
            self.assertEqual(len(blender_positions), point_count)
        elif usd_curve_type_basis == "cubic-bezier":
            control_point_count = 0
            usd_vert_counts = usd_curve.GetCurveVertexCountsAttr().Get()
            for i in range(0, usd_spline_count):
                if usd_cyclic == "nonperiodic":
                    control_point_count += (int(usd_vert_counts[i] / 3) + 1)
                else:
                    control_point_count += (int(usd_vert_counts[i] / 3))

            point_count = control_point_count
            self.assertEqual(len(blender_positions), point_count)
        elif usd_curve_type_basis == "cubic-bspline":
            point_count = len(usd_positions)
            self.assertEqual(len(blender_positions), point_count)

        # Check radius data. (note: the currently available bsplines have no radii)
        if usd_curve_type_basis == "cubic-bspline":
            return

        if not usd_curve.GetWidthsAttr().IsAuthored():
            return

        usd_width_interpolation = usd_curve.GetWidthsInterpolation()
        usd_radius = [w / 2 for w in usd_curve.GetWidthsAttr().Get()]
        blender_radius = [r.value for r in blender_curve.attributes["radius"].data]
        if usd_curve_type_basis == "linear-bezier":
            if usd_width_interpolation == "constant":
                usd_radius = usd_radius * point_count

            for i in range(0, len(blender_radius)):
                self.assertAlmostEqual(blender_radius[i], usd_radius[i], 2)

        elif usd_curve_type_basis == "cubic-bezier":
            if usd_width_interpolation == "constant":
                usd_radius = usd_radius * point_count

                for i in range(0, len(blender_radius)):
                    self.assertAlmostEqual(blender_radius[i], usd_radius[i], 2)
            elif usd_width_interpolation == "varying":
                # Do a quick min/max sanity check instead of reimplementing width interpolation
                usd_min = min(usd_radius)
                usd_max = max(usd_radius)
                blender_min = min(blender_radius)
                blender_max = max(blender_radius)

                self.assertAlmostEqual(blender_min, usd_min, 2)
                self.assertAlmostEqual(blender_max, usd_max, 2)
            elif usd_width_interpolation == "vertex":
                # Do a quick check to ensure radius has been set at all
                self.assertEqual(True, all([r > 0 and r < 1 for r in blender_radius]))

    def test_import_curves_linear(self):
        """Test importing linear curve variations."""

        infile = str(self.testdir / "usd_curve_linear_all.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        curves = [o for o in bpy.data.objects if o.type == 'CURVES']
        self.assertEqual(8, len(curves), f"Test scene {infile} should have 8 curves; found {len(curves)}")

        stage = Usd.Stage.Open(infile)

        blender_curve = bpy.data.objects["linear_nonperiodic_single_constant"].data
        usd_prim = stage.GetPrimAtPath("/root/linear_nonperiodic/single/linear_nonperiodic_single_constant")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["linear_nonperiodic_single_varying"].data
        usd_prim = stage.GetPrimAtPath("/root/linear_nonperiodic/single/linear_nonperiodic_single_varying")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["linear_nonperiodic_multiple_constant"].data
        usd_prim = stage.GetPrimAtPath("/root/linear_nonperiodic/multiple/linear_nonperiodic_multiple_constant")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["linear_nonperiodic_multiple_varying"].data
        usd_prim = stage.GetPrimAtPath("/root/linear_nonperiodic/multiple/linear_nonperiodic_multiple_varying")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["linear_periodic_single_constant"].data
        usd_prim = stage.GetPrimAtPath("/root/linear_periodic/single/linear_periodic_single_constant")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["linear_periodic_single_varying"].data
        usd_prim = stage.GetPrimAtPath("/root/linear_periodic/single/linear_periodic_single_varying")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["linear_periodic_multiple_constant"].data
        usd_prim = stage.GetPrimAtPath("/root/linear_periodic/multiple/linear_periodic_multiple_constant")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["linear_periodic_multiple_varying"].data
        usd_prim = stage.GetPrimAtPath("/root/linear_periodic/multiple/linear_periodic_multiple_varying")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

    def test_import_curves_bezier(self):
        """Test importing bezier curve variations."""

        infile = str(self.testdir / "usd_curve_bezier_all.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        curves = [o for o in bpy.data.objects if o.type == 'CURVES']
        self.assertEqual(12, len(curves), f"Test scene {infile} should have 12 curves; found {len(curves)}")

        stage = Usd.Stage.Open(infile)

        blender_curve = bpy.data.objects["bezier_nonperiodic_single_constant"].data
        usd_prim = stage.GetPrimAtPath("/root/bezier_nonperiodic/single/bezier_nonperiodic_single_constant")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["bezier_nonperiodic_single_varying"].data
        usd_prim = stage.GetPrimAtPath("/root/bezier_nonperiodic/single/bezier_nonperiodic_single_varying")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["bezier_nonperiodic_single_vertex"].data
        usd_prim = stage.GetPrimAtPath("/root/bezier_nonperiodic/single/bezier_nonperiodic_single_vertex")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["bezier_nonperiodic_multiple_constant"].data
        usd_prim = stage.GetPrimAtPath("/root/bezier_nonperiodic/multiple/bezier_nonperiodic_multiple_constant")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["bezier_nonperiodic_multiple_varying"].data
        usd_prim = stage.GetPrimAtPath("/root/bezier_nonperiodic/multiple/bezier_nonperiodic_multiple_varying")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["bezier_nonperiodic_multiple_vertex"].data
        usd_prim = stage.GetPrimAtPath("/root/bezier_nonperiodic/multiple/bezier_nonperiodic_multiple_vertex")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["bezier_periodic_single_constant"].data
        usd_prim = stage.GetPrimAtPath("/root/bezier_periodic/single/bezier_periodic_single_constant")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["bezier_periodic_single_varying"].data
        usd_prim = stage.GetPrimAtPath("/root/bezier_periodic/single/bezier_periodic_single_varying")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["bezier_periodic_single_vertex"].data
        usd_prim = stage.GetPrimAtPath("/root/bezier_periodic/single/bezier_periodic_single_vertex")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["bezier_periodic_multiple_constant"].data
        usd_prim = stage.GetPrimAtPath("/root/bezier_periodic/multiple/bezier_periodic_multiple_constant")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["bezier_periodic_multiple_varying"].data
        usd_prim = stage.GetPrimAtPath("/root/bezier_periodic/multiple/bezier_periodic_multiple_varying")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

        blender_curve = bpy.data.objects["bezier_periodic_multiple_vertex"].data
        usd_prim = stage.GetPrimAtPath("/root/bezier_periodic/multiple/bezier_periodic_multiple_vertex")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

    def test_import_curves_bspline(self):
        """Test importing bspline curve variations."""

        # Use the existing hair test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_particle_hair.blend"))
        testfile = str(self.tempdir / "usd_particle_hair.usda")
        res = bpy.ops.wm.usd_export(filepath=testfile, export_hair=True, evaluation_mode="RENDER")
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        stage = Usd.Stage.Open(testfile)

        blender_curve = bpy.data.objects["ParticleSystem"].data
        usd_prim = stage.GetPrimAtPath("/root/Sphere/ParticleSystem")
        self.check_curve(blender_curve, UsdGeom.BasisCurves(usd_prim))

    def test_import_point_instancer(self):
        """Test importing a typical point instancer setup."""

        infile = str(self.testdir / "usd_nested_point_instancer.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        pointclouds = [o for o in bpy.data.objects if o.type == 'POINTCLOUD']
        self.assertEqual(
            2,
            len(pointclouds),
            f"Test scene {infile} should have 2 pointclouds; found {len(pointclouds)}")

        vertical_points = len(bpy.data.pointclouds['verticalpoints'].attributes["position"].data)
        horizontal_points = len(bpy.data.pointclouds['horizontalpoints'].attributes["position"].data)
        self.assertEqual(3, vertical_points)
        self.assertEqual(2, horizontal_points)

    def test_import_point_instancer_animation(self):
        """Test importing an animated point instancer setup."""

        infile = str(self.testdir / "usd_point_instancer_anim.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        prev_unique_positions = set()
        prev_unique_scales = set()
        prev_unique_quats = set()

        # Check all frames to ensure instances are moving correctly
        for frame in range(1, 5):
            bpy.context.scene.frame_set(frame)
            depsgraph = bpy.context.evaluated_depsgraph_get()

            # Gather the instance data in a set so we can detect unique values
            unique_positions = set()
            unique_scales = set()
            unique_quats = set()
            mesh_count = 0
            for inst in depsgraph.object_instances:
                if inst.is_instance and inst.object.type == 'MESH':
                    mesh_count += 1
                    unique_positions.add(tuple(self.round_vector(inst.matrix_world.to_translation())))
                    unique_scales.add(tuple(self.round_vector(inst.matrix_world.to_scale(), 1)))
                    unique_quats.add(tuple(self.round_vector(inst.matrix_world.to_quaternion())))

            # There should be 6 total mesh instances
            self.assertEqual(mesh_count, 6)

            # Positions: All positions should be unique during each frame.
            # Scale and Orientation: One unique value on frame 1. Subsequent frames have different
            # combinations of unique values.
            self.assertEqual(len(unique_positions), 6, f"Frame {frame}: positions are unexpected")
            if frame == 1:
                self.assertEqual(len(unique_scales), 1, f"Frame {frame}: scales are unexpected")
                self.assertEqual(len(unique_quats), 1, f"Frame {frame}: orientations are unexpected")
            else:
                self.assertEqual(len(unique_scales), 2, f"Frame {frame}: scales are unexpected")
                self.assertEqual(len(unique_quats), 3, f"Frame {frame}: orientations are unexpected")

            # Every frame is different. Ensure that the current frame's values do NOT match the
            # previous frame's data.
            self.assertNotEqual(unique_positions, prev_unique_positions)
            self.assertNotEqual(unique_scales, prev_unique_scales)
            self.assertNotEqual(unique_quats, prev_unique_quats)

            prev_unique_positions = unique_positions
            prev_unique_scales = unique_scales
            prev_unique_quats = unique_quats

    def test_import_light_types(self):
        """Test importing light types and attributes."""

        def rename_active(new_name):
            active_ob = bpy.context.view_layer.objects.active
            active_ob.name = new_name
            active_ob.data.name = new_name

        # Use the current scene to first create and export the lights
        bpy.ops.object.light_add(type='POINT', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        bpy.context.active_object.data.energy = 2
        bpy.context.active_object.data.shadow_soft_size = 2.2

        bpy.ops.object.light_add(type='SPOT', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        rename_active("Spot")
        bpy.context.active_object.data.energy = 3
        bpy.context.active_object.data.shadow_soft_size = 3.3
        bpy.context.active_object.data.spot_blend = 0.25
        bpy.context.active_object.data.spot_size = math.radians(60)

        bpy.ops.object.light_add(type='SPOT', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        rename_active("Spot_point")
        bpy.context.active_object.data.energy = 3.5
        bpy.context.active_object.data.shadow_soft_size = 0
        bpy.context.active_object.data.spot_blend = 0.25
        bpy.context.active_object.data.spot_size = math.radians(60)

        bpy.ops.object.light_add(type='SUN', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        bpy.context.active_object.data.energy = 4
        bpy.context.active_object.data.angle = math.radians(1)

        bpy.ops.object.light_add(type='AREA', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        rename_active("Area_rect")
        bpy.context.active_object.data.energy = 5
        bpy.context.active_object.data.shape = 'RECTANGLE'
        bpy.context.active_object.data.size = 0.5
        bpy.context.active_object.data.size_y = 1.5

        bpy.ops.object.light_add(type='AREA', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        rename_active("Area_square")
        bpy.context.active_object.data.energy = 5.5
        bpy.context.active_object.data.shape = 'SQUARE'
        bpy.context.active_object.data.size = 0.7

        bpy.ops.object.light_add(type='AREA', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        rename_active("Area_disk")
        bpy.context.active_object.data.energy = 6
        bpy.context.active_object.data.shape = 'DISK'
        bpy.context.active_object.data.size = 2

        bpy.ops.object.light_add(type='AREA', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        rename_active("Area_ellipse")
        bpy.context.active_object.data.energy = 6.5
        bpy.context.active_object.data.shape = 'ELLIPSE'
        bpy.context.active_object.data.size = 3
        bpy.context.active_object.data.size_y = 5

        test_path = self.tempdir / "temp_lights.usda"
        res = bpy.ops.wm.usd_export(filepath=str(test_path), evaluation_mode="RENDER")
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {test_path}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        infile = str(test_path)
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        lights = [o for o in bpy.data.objects if o.type == 'LIGHT']
        self.assertEqual(8, len(lights), f"Test scene {infile} should have 8 lights; found {len(lights)}")

        blender_light = bpy.data.lights["Point"]
        self.assertAlmostEqual(blender_light.energy, 2, 3)
        self.assertAlmostEqual(blender_light.shadow_soft_size, 2.2, 3)

        blender_light = bpy.data.lights["Spot"]
        self.assertAlmostEqual(blender_light.energy, 3, 3)
        self.assertAlmostEqual(blender_light.shadow_soft_size, 3.3, 3)
        self.assertAlmostEqual(blender_light.spot_blend, 0.25, 3)
        self.assertAlmostEqual(blender_light.spot_size, math.radians(60), 3)

        blender_light = bpy.data.lights["Spot_point"]
        self.assertAlmostEqual(blender_light.energy, 3.5, 3)
        self.assertAlmostEqual(blender_light.shadow_soft_size, 0, 3)
        self.assertAlmostEqual(blender_light.spot_blend, 0.25, 3)
        self.assertAlmostEqual(blender_light.spot_size, math.radians(60), 3)

        blender_light = bpy.data.lights["Sun"]
        self.assertAlmostEqual(blender_light.energy, 4, 3)
        self.assertAlmostEqual(blender_light.angle, math.radians(1), 3)

        blender_light = bpy.data.lights["Area_rect"]
        self.assertAlmostEqual(blender_light.energy, 5, 3)
        self.assertEqual(blender_light.shape, 'RECTANGLE')
        self.assertAlmostEqual(blender_light.size, 0.5, 3)
        self.assertAlmostEqual(blender_light.size_y, 1.5, 3)

        blender_light = bpy.data.lights["Area_square"]
        self.assertAlmostEqual(blender_light.energy, 5.5, 3)
        self.assertEqual(blender_light.shape, 'RECTANGLE')  # We read as rectangle to mirror what USD supports
        self.assertAlmostEqual(blender_light.size, 0.7, 3)

        blender_light = bpy.data.lights["Area_disk"]
        self.assertAlmostEqual(blender_light.energy, 6, 3)
        self.assertEqual(blender_light.shape, 'DISK')
        self.assertAlmostEqual(blender_light.size, 2, 3)

        blender_light = bpy.data.lights["Area_ellipse"]
        self.assertAlmostEqual(blender_light.energy, 6.5, 3)
        self.assertEqual(blender_light.shape, 'DISK')  # We read as disk to mirror what USD supports
        self.assertAlmostEqual(blender_light.size, 4, 3)

    def test_import_dome_lights(self):
        """Test importing dome lights and verify their rotations."""

        # Test files and their expected EnvironmentTexture Mapping rotation values
        tests = [
            ("usd_dome_light_1_stageZ_poleY.usda", [0.0, 0.0, 0.0]),
            ("usd_dome_light_1_stageZ_poleZ.usda", [0.0, -1.5708, 0.0]),
            ("usd_dome_light_1_stageY_poleDefault.usda", [-1.5708, 0.0, 0.0])
        ]

        for test_name, expected_rot in tests:
            bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))

            infile = str(self.testdir / test_name)
            res = bpy.ops.wm.usd_import(filepath=infile)
            self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

            # Validate that the Mapping node on the World Material is set to the correct rotation
            world = bpy.data.worlds["World"]
            node = world.node_tree.nodes["Mapping"]
            self.assertEqual(
                self.round_vector(node.inputs[2].default_value), expected_rot, f"Incorrect rotation for {test_name}")

    def check_attribute(self, blender_data, attribute_name, domain, data_type, elements_len):
        attr = blender_data.attributes[attribute_name]
        self.assertEqual(attr.domain, domain)
        self.assertEqual(attr.data_type, data_type)
        self.assertEqual(len(attr.data), elements_len)

    def check_attribute_missing(self, blender_data, attribute_name):
        self.assertFalse(attribute_name in blender_data.attributes)

    def test_import_attributes(self):
        """Test importing objects with all attribute data types."""

        # Use the existing attributes test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_attribute_test.blend"))

        testfile = str(self.tempdir / "usd_attribute_test.usda")
        res = bpy.ops.wm.usd_export(filepath=testfile, evaluation_mode="RENDER")
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # Verify all attributes on the Mesh
        # Note: USD does not support signed 8-bit types so there is
        #       currently no equivalent to Blender's INT8 data type
        # TODO: Blender is missing support for reading USD matrix data types
        mesh = bpy.data.objects["Mesh"].data

        self.check_attribute(mesh, "p_bool", 'POINT', 'BOOLEAN', 4)
        self.check_attribute(mesh, "p_int8", 'POINT', 'INT8', 4)
        self.check_attribute(mesh, "p_int32", 'POINT', 'INT', 4)
        self.check_attribute(mesh, "p_float", 'POINT', 'FLOAT', 4)
        self.check_attribute(mesh, "p_byte_color", 'POINT', 'FLOAT_COLOR', 4)
        self.check_attribute(mesh, "p_color", 'POINT', 'FLOAT_COLOR', 4)
        self.check_attribute(mesh, "p_vec2", 'CORNER', 'FLOAT2', 4)  # TODO: Bug - wrong domain
        self.check_attribute(mesh, "p_vec3", 'POINT', 'FLOAT_VECTOR', 4)
        self.check_attribute(mesh, "p_quat", 'POINT', 'QUATERNION', 4)
        self.check_attribute_missing(mesh, "p_mat4x4")

        self.check_attribute(mesh, "f_bool", 'FACE', 'BOOLEAN', 1)
        self.check_attribute(mesh, "f_int8", 'FACE', 'INT8', 1)
        self.check_attribute(mesh, "f_int32", 'FACE', 'INT', 1)
        self.check_attribute(mesh, "f_float", 'FACE', 'FLOAT', 1)
        self.check_attribute(mesh, "f_byte_color", 'FACE', 'FLOAT_COLOR', 1)
        self.check_attribute(mesh, "f_color", 'FACE', 'FLOAT_COLOR', 1)
        self.check_attribute(mesh, "f_vec2", 'FACE', 'FLOAT2', 1)
        self.check_attribute(mesh, "f_vec3", 'FACE', 'FLOAT_VECTOR', 1)
        self.check_attribute(mesh, "f_quat", 'FACE', 'QUATERNION', 1)
        self.check_attribute_missing(mesh, "f_mat4x4")

        self.check_attribute(mesh, "fc_bool", 'CORNER', 'BOOLEAN', 4)
        self.check_attribute(mesh, "fc_int8", 'CORNER', 'INT8', 4)
        self.check_attribute(mesh, "fc_int32", 'CORNER', 'INT', 4)
        self.check_attribute(mesh, "fc_float", 'CORNER', 'FLOAT', 4)
        self.check_attribute(mesh, "fc_byte_color", 'CORNER', 'FLOAT_COLOR', 4)
        self.check_attribute(mesh, "fc_color", 'CORNER', 'FLOAT_COLOR', 4)
        self.check_attribute(mesh, "displayColor", 'CORNER', 'FLOAT_COLOR', 4)
        self.check_attribute(mesh, "fc_vec2", 'CORNER', 'FLOAT2', 4)
        self.check_attribute(mesh, "fc_vec3", 'CORNER', 'FLOAT_VECTOR', 4)
        self.check_attribute(mesh, "fc_quat", 'CORNER', 'QUATERNION', 4)
        self.check_attribute_missing(mesh, "fc_mat4x4")

        # Find the non "bezier" Curves object -- Has 2 curves (12 vertices each)
        all_curves = [o for o in bpy.data.objects if o.type == 'CURVES']
        curves = [o for o in all_curves if not o.parent.name.startswith("Curve_bezier")]
        curves = curves[0].data

        self.check_attribute(curves, "p_bool", 'POINT', 'BOOLEAN', 24)
        self.check_attribute(curves, "p_int8", 'POINT', 'INT8', 24)
        self.check_attribute(curves, "p_int32", 'POINT', 'INT', 24)
        self.check_attribute(curves, "p_float", 'POINT', 'FLOAT', 24)
        self.check_attribute(curves, "p_byte_color", 'POINT', 'FLOAT_COLOR', 24)
        self.check_attribute(curves, "p_color", 'POINT', 'FLOAT_COLOR', 24)
        self.check_attribute(curves, "p_vec2", 'POINT', 'FLOAT2', 24)
        self.check_attribute(curves, "p_vec3", 'POINT', 'FLOAT_VECTOR', 24)
        self.check_attribute(curves, "p_quat", 'POINT', 'QUATERNION', 24)
        self.check_attribute_missing(curves, "p_mat4x4")

        self.check_attribute(curves, "sp_bool", 'CURVE', 'BOOLEAN', 2)
        self.check_attribute(curves, "sp_int8", 'CURVE', 'INT8', 2)
        self.check_attribute(curves, "sp_int32", 'CURVE', 'INT', 2)
        self.check_attribute(curves, "sp_float", 'CURVE', 'FLOAT', 2)
        self.check_attribute(curves, "sp_byte_color", 'CURVE', 'FLOAT_COLOR', 2)
        self.check_attribute(curves, "sp_color", 'CURVE', 'FLOAT_COLOR', 2)
        self.check_attribute(curves, "sp_vec2", 'CURVE', 'FLOAT2', 2)
        self.check_attribute(curves, "sp_vec3", 'CURVE', 'FLOAT_VECTOR', 2)
        self.check_attribute(curves, "sp_quat", 'CURVE', 'QUATERNION', 2)
        self.check_attribute_missing(curves, "sp_mat4x4")

        # Find the "bezier" Curves object -- Has 3 curves (2, 3, and 5 control points)
        curves = [o for o in all_curves if o.parent.name.startswith("Curve_bezier")]
        curves = curves[0].data

        self.check_attribute(curves, "p_bool", 'POINT', 'BOOLEAN', 10)
        self.check_attribute(curves, "p_int8", 'POINT', 'INT8', 10)
        self.check_attribute(curves, "p_int32", 'POINT', 'INT', 10)
        self.check_attribute(curves, "p_float", 'POINT', 'FLOAT', 10)
        self.check_attribute(curves, "p_byte_color", 'POINT', 'FLOAT_COLOR', 10)
        self.check_attribute(curves, "p_color", 'POINT', 'FLOAT_COLOR', 10)
        self.check_attribute(curves, "p_vec2", 'POINT', 'FLOAT2', 10)
        self.check_attribute(curves, "p_vec3", 'POINT', 'FLOAT_VECTOR', 10)
        self.check_attribute(curves, "p_quat", 'POINT', 'QUATERNION', 10)
        self.check_attribute_missing(curves, "p_mat4x4")

        self.check_attribute(curves, "sp_bool", 'CURVE', 'BOOLEAN', 3)
        self.check_attribute(curves, "sp_int8", 'CURVE', 'INT8', 3)
        self.check_attribute(curves, "sp_int32", 'CURVE', 'INT', 3)
        self.check_attribute(curves, "sp_float", 'CURVE', 'FLOAT', 3)
        self.check_attribute(curves, "sp_byte_color", 'CURVE', 'FLOAT_COLOR', 3)
        self.check_attribute(curves, "sp_color", 'CURVE', 'FLOAT_COLOR', 3)
        self.check_attribute(curves, "sp_vec2", 'CURVE', 'FLOAT2', 3)
        self.check_attribute(curves, "sp_vec3", 'CURVE', 'FLOAT_VECTOR', 3)
        self.check_attribute(curves, "sp_quat", 'CURVE', 'QUATERNION', 3)
        self.check_attribute_missing(curves, "sp_mat4x4")

    def test_import_attributes_varying(self):
        """Test importing objects with time-varying positions, velocities, and attributes."""

        # Use the existing attributes test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_attribute_varying_test.blend"))
        for frame in range(1, 16):
            bpy.context.scene.frame_set(frame)
        bpy.context.scene.frame_set(1)

        testfile = str(self.tempdir / "usd_attribute_varying_test.usda")
        res = bpy.ops.wm.usd_export(filepath=testfile, export_animation=True, evaluation_mode="RENDER")
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        stage = Usd.Stage.Open(testfile)

        #
        # Validate Mesh data
        #
        blender_mesh = [bpy.data.objects["mesh1"], bpy.data.objects["mesh2"], bpy.data.objects["mesh3"]]
        usd_mesh = [UsdGeom.Mesh(stage.GetPrimAtPath("/root/mesh1/mesh1")),
                    UsdGeom.Mesh(stage.GetPrimAtPath("/root/mesh2/mesh2")),
                    UsdGeom.Mesh(stage.GetPrimAtPath("/root/mesh3/mesh3"))]
        mesh_num = len(blender_mesh)

        # A MeshSequenceCache modifier should be present on every imported object
        for i in range(0, mesh_num):
            self.assertTrue(len(blender_mesh[i].modifiers) == 1 and blender_mesh[i].modifiers[0].type ==
                            'MESH_SEQUENCE_CACHE', f"{blender_mesh[i].name} has incorrect modifiers")

        # Compare Blender and USD data against each other for every frame
        for frame in range(1, 16):
            bpy.context.scene.frame_set(frame)
            depsgraph = bpy.context.evaluated_depsgraph_get()
            for i in range(0, mesh_num):
                blender_mesh[i] = bpy.data.objects["mesh" + str(i + 1)].evaluated_get(depsgraph)

            # Check positions, velocity, and test data
            for i in range(0, mesh_num):
                blender_pos_data = [self.round_vector(d.vector)
                                    for d in blender_mesh[i].data.attributes["position"].data]
                blender_vel_data = [self.round_vector(d.vector)
                                    for d in blender_mesh[i].data.attributes["velocity"].data]
                blender_test_data = [round(d.value, 5) for d in blender_mesh[i].data.attributes["test"].data]
                usd_pos_data = [self.round_vector(d) for d in usd_mesh[i].GetPointsAttr().Get(frame)]
                usd_vel_data = [self.round_vector(d) for d in usd_mesh[i].GetVelocitiesAttr().Get(frame)]
                usd_test_data = [round(d, 5) for d in UsdGeom.PrimvarsAPI(usd_mesh[i]).GetPrimvar("test").Get(frame)]

                self.assertEqual(
                    blender_pos_data,
                    usd_pos_data,
                    f"Frame {frame}: {blender_mesh[i].name} positions do not match")
                self.assertEqual(
                    blender_vel_data,
                    usd_vel_data,
                    f"Frame {frame}: {blender_mesh[i].name} velocities do not match")
                self.assertEqual(
                    blender_test_data,
                    usd_test_data,
                    f"Frame {frame}: {blender_mesh[i].name} test attributes do not match")

        #
        # Validate Point Cloud data
        #
        blender_pointclouds = [
            bpy.data.objects["PointCloud"],
            bpy.data.objects["PointCloud.001"],
            bpy.data.objects["PointCloud.002"],
            bpy.data.objects["PointCloud.003"]]
        usd_points = [UsdGeom.Points(stage.GetPrimAtPath("/root/pointcloud1/PointCloud")),
                      UsdGeom.Points(stage.GetPrimAtPath("/root/pointcloud2/PointCloud")),
                      UsdGeom.Points(stage.GetPrimAtPath("/root/pointcloud3/PointCloud")),
                      UsdGeom.Points(stage.GetPrimAtPath("/root/pointcloud4/PointCloud"))]
        pointclouds_num = len(blender_pointclouds)

        # Workaround: GeometrySet processing loses the data-block name on export. This is why the
        # .001 etc. names are being used above. Since we need the order of Blender objects to match
        # the order of USD prims, sort by the Y location to make them match in our test setup.
        blender_pointclouds.sort(key=lambda ob: ob.location.y)

        # A MeshSequenceCache modifier should be present on every imported object
        for i in range(0, pointclouds_num):
            self.assertTrue(len(blender_pointclouds[i].modifiers) == 1 and blender_pointclouds[i].modifiers[0].type ==
                            'MESH_SEQUENCE_CACHE', f"{blender_pointclouds[i].name} has incorrect modifiers")

        # Compare Blender and USD data against each other for every frame
        for frame in range(1, 16):
            bpy.context.scene.frame_set(frame)
            depsgraph = bpy.context.evaluated_depsgraph_get()
            for i in range(0, mesh_num):
                blender_pointclouds[i] = blender_pointclouds[i].evaluated_get(depsgraph)

            # Check positions, velocity, radius, and test data
            for i in range(0, mesh_num):
                blender_pos_data = [self.round_vector(d.vector)
                                    for d in blender_pointclouds[i].data.attributes["position"].data]
                blender_vel_data = [self.round_vector(d.vector)
                                    for d in blender_pointclouds[i].data.attributes["velocity"].data]
                blender_radius_data = [round(d.value, 5) for d in blender_pointclouds[i].data.attributes["radius"].data]
                blender_test_data = [round(d.value, 5) for d in blender_pointclouds[i].data.attributes["test"].data]
                usd_pos_data = [self.round_vector(d) for d in usd_points[i].GetPointsAttr().Get(frame)]
                usd_vel_data = [self.round_vector(d) for d in usd_points[i].GetVelocitiesAttr().Get(frame)]
                usd_radius_data = [round(d / 2, 5) for d in usd_points[i].GetWidthsAttr().Get(frame)]
                usd_test_data = [round(d, 5) for d in UsdGeom.PrimvarsAPI(usd_points[i]).GetPrimvar("test").Get(frame)]

                name = usd_points[i].GetPath().GetParentPath().name
                self.assertEqual(
                    blender_pos_data,
                    usd_pos_data,
                    f"Frame {frame}: {name} positions do not match")
                self.assertEqual(
                    blender_vel_data,
                    usd_vel_data,
                    f"Frame {frame}: {name} velocities do not match")
                self.assertEqual(
                    blender_radius_data,
                    usd_radius_data,
                    f"Frame {frame}: {name} radii do not match")
                self.assertEqual(
                    blender_test_data,
                    usd_test_data,
                    f"Frame {frame}: {name} test attributes do not match")

        #
        # Validate Curves data
        #
        blender_curves = [
            bpy.data.objects["Curves"],
            bpy.data.objects["Curves.001"],
            bpy.data.objects["Curves.002"],
            bpy.data.objects["Curves.003"]]
        usd_curves = [UsdGeom.BasisCurves(stage.GetPrimAtPath("/root/curves_plane1/curves1/Curves")),
                      UsdGeom.BasisCurves(stage.GetPrimAtPath("/root/curves_plane2/curves2/Curves")),
                      UsdGeom.BasisCurves(stage.GetPrimAtPath("/root/curves_plane3/curves3/Curves")),
                      UsdGeom.BasisCurves(stage.GetPrimAtPath("/root/curves_plane4/curves4/Curves"))]
        curves_num = len(blender_curves)

        # Workaround: GeometrySet processing loses the data-block name on export. This is why the
        # .001 etc. names are being used above. Since we need the order of Blender objects to match
        # the order of USD prims, sort by the Y location to make them match in our test setup.
        blender_curves.sort(key=lambda ob: ob.parent.location.y)

        # A MeshSequenceCache modifier should be present on every imported object
        for i in range(0, curves_num):
            self.assertTrue(len(blender_curves[i].modifiers) == 1 and blender_curves[i].modifiers[0].type ==
                            'MESH_SEQUENCE_CACHE', f"{blender_curves[i].name} has incorrect modifiers")

        # Compare Blender and USD data against each other for every frame
        for frame in range(1, 16):
            bpy.context.scene.frame_set(frame)
            depsgraph = bpy.context.evaluated_depsgraph_get()
            for i in range(0, mesh_num):
                blender_curves[i] = blender_curves[i].evaluated_get(depsgraph)

            # Check positions, velocity, radius, and test data
            for i in range(0, mesh_num):
                blender_pos_data = [self.round_vector(d.vector)
                                    for d in blender_curves[i].data.attributes["position"].data]
                blender_vel_data = [self.round_vector(d.vector)
                                    for d in blender_curves[i].data.attributes["velocity"].data]
                blender_radius_data = [round(d.value, 5) for d in blender_curves[i].data.attributes["radius"].data]
                blender_test_data = [round(d.value, 5) for d in blender_curves[i].data.attributes["test"].data]
                usd_pos_data = [self.round_vector(d) for d in usd_curves[i].GetPointsAttr().Get(frame)]
                usd_vel_data = [self.round_vector(d) for d in usd_curves[i].GetVelocitiesAttr().Get(frame)]
                usd_radius_data = [round(d / 2, 5) for d in usd_curves[i].GetWidthsAttr().Get(frame)]
                usd_test_data = [round(d, 5) for d in UsdGeom.PrimvarsAPI(usd_curves[i]).GetPrimvar("test").Get(frame)]

                name = usd_curves[i].GetPath().GetParentPath().name
                self.assertEqual(
                    blender_pos_data,
                    usd_pos_data,
                    f"Frame {frame}: {name} positions do not match")
                self.assertEqual(
                    blender_vel_data,
                    usd_vel_data,
                    f"Frame {frame}: {name} velocities do not match")
                self.assertEqual(
                    blender_radius_data,
                    usd_radius_data,
                    f"Frame {frame}: {name} radii do not match")
                self.assertEqual(
                    blender_test_data,
                    usd_test_data,
                    f"Frame {frame}: {name} test attributes do not match")

    def test_import_shapes(self):
        """Test importing USD Shape prims with time-varying attributes."""

        infile = str(self.testdir / "usd_shapes_test.usda")
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        # Ensure we find the expected number of mesh objects
        blender_objects = [ob for ob in bpy.data.objects if ob.type == 'MESH']
        self.assertEqual(
            9,
            len(blender_objects),
            f"Test scene {infile} should have 9 mesh objects; found {len(blender_objects)}")

        # A MeshSequenceCache modifier should be present on every imported object
        for ob in blender_objects:
            self.assertTrue(len(ob.modifiers) == 1 and ob.modifiers[0].type ==
                            'MESH_SEQUENCE_CACHE', f"{ob.name} has incorrect modifiers")

        # Check that the shape with the color attribute properly updates and has correct values
        def get_first_color_value(blender_object, frame):
            bpy.context.scene.frame_set(frame)
            depsgraph = bpy.context.evaluated_depsgraph_get()
            return blender_object.evaluated_get(depsgraph).data.attributes["displayColor"].data.values()[0].color
        blender_color = get_first_color_value(bpy.data.objects["capsule_color"], 1)
        self.assertEqual(self.round_vector(blender_color), [0.8, 1.0, 0.0, 1.0])
        blender_color = get_first_color_value(bpy.data.objects["capsule_color"], 2)
        self.assertEqual(self.round_vector(blender_color), [0.1, 0.8, 0.0, 1.0])

    def test_import_collection_creation(self):
        """Test that the 'create_collection' option functions correctly."""

        # Any USD file will do
        infile = str(self.testdir / "usd_shapes_test.usda")

        # Import the file more than once to ensure the auto generated Collection name is unique
        # and no naming conflicts occur
        res = bpy.ops.wm.usd_import(filepath=infile, create_collection=True)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")
        res = bpy.ops.wm.usd_import(filepath=infile, create_collection=True)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        # Validate the correct user count for each Collection and ensure the objects were
        # placed inside each one.
        self.assertEqual(len(bpy.data.collections), 2)
        self.assertEqual(bpy.data.collections["Usd Shapes Test"].users, 1)
        self.assertEqual(bpy.data.collections["Usd Shapes Test.001"].users, 1)
        self.assertEqual(len(bpy.data.collections["Usd Shapes Test"].all_objects), 10)
        self.assertEqual(len(bpy.data.collections["Usd Shapes Test.001"].all_objects), 10)

    def test_import_id_props(self):
        """Test importing object and data IDProperties."""

        # Create our set of ID's with all relevant IDProperty types/values that we support
        bpy.ops.object.empty_add()
        bpy.ops.object.light_add()
        bpy.ops.object.camera_add()
        bpy.ops.mesh.primitive_plane_add()

        ids = [ob if ob.type == 'EMPTY' else ob.data for ob in bpy.data.objects]
        properties = [
            True, "string", 1, 2.0, [1, 2], [1, 2, 3], [1, 2, 3, 4], [1.0, 2.0], [1.0, 2.0, 3.0], [1.0, 2.0, 3.0, 4.0]
        ]
        for id in ids:
            for i, p in enumerate(properties):
                prop_name = "prop" + str(i)
                id[prop_name] = p

        # Export out this scene twice so we can test both the default "userProperties" namespace as
        # well as a custom namespace
        test_path1 = self.tempdir / "temp_idprops_userProperties_test.usda"
        res = bpy.ops.wm.usd_export(filepath=str(test_path1), evaluation_mode="RENDER")
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {test_path1}")

        custom_namespace = "customns"
        test_path2 = self.tempdir / "temp_idprops_customns_test.usda"
        res = bpy.ops.wm.usd_export(
            filepath=str(test_path2),
            custom_properties_namespace=custom_namespace,
            evaluation_mode="RENDER")
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {test_path2}")

        # Also write out another file using attribute types not natively writable by Blender
        test_path3 = self.tempdir / "temp_idprops_extended_test.usda"
        stage = Usd.Stage.CreateNew(str(test_path3))
        xform = UsdGeom.Xform.Define(stage, '/empty')
        xform.GetPrim().CreateAttribute("prop0", Sdf.ValueTypeNames.Half).Set(0.5)
        xform.GetPrim().CreateAttribute("prop1", Sdf.ValueTypeNames.Float).Set(1.5)
        xform.GetPrim().CreateAttribute("prop2", Sdf.ValueTypeNames.Token).Set("tokenstring")
        xform.GetPrim().CreateAttribute("prop3", Sdf.ValueTypeNames.Asset).Set("assetstring")
        xform.GetPrim().CreateAttribute("prop4", Sdf.ValueTypeNames.Half2).Set(Gf.Vec2h(0, 1))
        xform.GetPrim().CreateAttribute("prop5", Sdf.ValueTypeNames.Half3).Set(Gf.Vec3h(0, 1, 2))
        xform.GetPrim().CreateAttribute("prop6", Sdf.ValueTypeNames.Half4).Set(Gf.Vec4h(0, 1, 2, 3))
        xform.GetPrim().CreateAttribute("prop7", Sdf.ValueTypeNames.Float2).Set(Gf.Vec2f(0, 1))
        xform.GetPrim().CreateAttribute("prop8", Sdf.ValueTypeNames.Float3).Set(Gf.Vec3f(0, 1, 2))
        xform.GetPrim().CreateAttribute("prop9", Sdf.ValueTypeNames.Float4).Set(Gf.Vec4f(0, 1, 2, 3))
        stage.GetRootLayer().Save()

        # Helper functions to check IDProperty validity
        import idprop

        def assert_all_props_present(properties, ns):
            ids = [ob if ob.type == 'EMPTY' else ob.data for ob in bpy.data.objects]
            for id in ids:
                for i, p in enumerate(properties):
                    prop_name = (ns + ":" if ns != "" else "") + "prop" + str(i)
                    prop = id[prop_name]
                    value = prop.to_list() if type(prop) is idprop.types.IDPropertyArray else prop
                    self.assertEqual(p, value, f"Property {prop_name} is incorrect")

        def assert_no_props_present(properties, ns):
            ids = [ob if ob.type == 'EMPTY' else ob.data for ob in bpy.data.objects]
            for id in ids:
                for i, p in enumerate(properties):
                    prop_name = (ns + ":" if ns != "" else "") + "prop" + str(i)
                    self.assertTrue(id.get(prop_name) is None, f"Property {prop_name} should not be present")

        # Reload the empty file and test the relevant combinations of namespaces and import modes

        infile = str(test_path1)
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=infile, property_import_mode='USER')
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")
        self.assertEqual(len(bpy.data.objects), 4)
        assert_all_props_present(properties, "")

        infile = str(test_path1)
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=infile, property_import_mode='NONE')
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")
        self.assertEqual(len(bpy.data.objects), 4)
        assert_no_props_present(properties, "")

        infile = str(test_path2)
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=infile, property_import_mode='ALL')
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")
        self.assertEqual(len(bpy.data.objects), 4)
        assert_all_props_present(properties, custom_namespace)

        infile = str(test_path2)
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=infile, property_import_mode='USER')
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")
        self.assertEqual(len(bpy.data.objects), 4)
        assert_no_props_present(properties, custom_namespace)

        infile = str(test_path3)
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=infile, property_import_mode='ALL')
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")
        self.assertEqual(len(bpy.data.objects), 1)
        properties = [
            0.5, 1.5, "tokenstring", "assetstring", [0, 1], [0, 1, 2], [0, 1, 2, 3], [0, 1], [0, 1, 2], [0, 1, 2, 3]
        ]
        assert_all_props_present(properties, "")

    def test_import_usdz_image_processing(self):
        """Test importing of images from USDZ files in various ways."""

        # USDZ processing needs the destination directory to exist
        self.tempdir.mkdir(parents=True, exist_ok=True)

        # Use the existing materials test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_export.blend"))
        usdz1 = str(self.tempdir / "usd_materials_export.usdz")
        res = bpy.ops.wm.usd_export(filepath=usdz1, export_materials=True)
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {usdz1}")

        usdz2 = str(self.tempdir / "usd_materials_export_downscaled.usdz")
        res = bpy.ops.wm.usd_export(
            filepath=usdz2,
            export_materials=True,
            usdz_downscale_size='CUSTOM',
            usdz_downscale_custom_size=128)
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {usdz2}")

        def check_image(name, tiles_num, size, is_packed):
            self.assertTrue(name in bpy.data.images, f"Could not find '{name}'")

            image = bpy.data.images[name]
            self.assertEqual(len(image.tiles), tiles_num)
            self.assertEqual(image.packed_file is not None, is_packed)
            for tile in range(0, tiles_num):
                self.assertEqual(image.tiles[tile].size[0], size)
                self.assertEqual(image.tiles[tile].size[1], size)

        def check_materials():
            self.assertTrue("Clip_With_LessThanInvert" in bpy.data.materials)
            self.assertTrue("Clip_With_Round" in bpy.data.materials)
            self.assertTrue("Material" in bpy.data.materials)
            self.assertTrue("NormalMap" in bpy.data.materials)
            self.assertTrue("NormalMap_Scale_Bias" in bpy.data.materials)
            self.assertTrue("Transforms" in bpy.data.materials)
            self.assertEqual(len(bpy.data.materials), 6)

        # Reload the empty file and import back in using IMPORT_PACK
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=usdz1, import_textures_mode='IMPORT_PACK')
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {usdz1}")

        self.assertEqual(len(bpy.data.images), 4)
        check_image("test_grid_<UDIM>.png", 2, 1024, True)
        check_image("test_normal.exr", 1, 128, True)
        check_image("test_normal_invertY.exr", 1, 128, True)
        check_image("color_0C0C0C.exr", 1, 1, True)
        check_materials()

        # Reload the empty file and import back in using IMPORT_COPY
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(
            filepath=usdz2,
            import_textures_mode='IMPORT_COPY',
            import_textures_dir=str(
                self.tempdir))
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {usdz2}")

        self.assertEqual(len(bpy.data.images), 4)
        check_image("test_grid_<UDIM>.png", 2, 128, False)
        check_image("test_normal.exr", 1, 128, False)
        check_image("test_normal_invertY.exr", 1, 128, False)
        check_image("color_0C0C0C.exr", 1, 1, False)
        check_materials()

    def test_get_prim_map_parent_xform_not_merged(self):
        bpy.utils.register_class(GetPrimMapUsdImportHook)
        bpy.ops.wm.usd_import(filepath=str(self.testdir / "usd_name_property_template.usda"), merge_parent_xform=False)
        prim_map = GetPrimMapUsdImportHook.prim_map
        bpy.utils.unregister_class(GetPrimMapUsdImportHook)

        expected_prim_map = {
            Sdf.Path('/Cube'): [bpy.data.objects["Cube.002"], bpy.data.meshes["Cube.002"]],
            Sdf.Path('/XformThenCube'): [bpy.data.objects["XformThenCube"]],
            Sdf.Path('/XformThenCube/Cube'): [bpy.data.objects["Cube"], bpy.data.meshes["Cube"]],
            Sdf.Path('/XformThenXformCube'): [bpy.data.objects["XformThenXformCube"]],
            Sdf.Path('/XformThenXformCube/XformIntermediate'): [bpy.data.objects["XformIntermediate"]],
            Sdf.Path('/XformThenXformCube/XformIntermediate/Cube'): [bpy.data.objects["Cube.001"], bpy.data.meshes["Cube.001"]],
            Sdf.Path('/Material'): [bpy.data.materials["Material"]],
        }

        self.assertDictEqual(prim_map, expected_prim_map)

    def test_get_prim_map_parent_xform_merged(self):
        bpy.utils.register_class(GetPrimMapUsdImportHook)
        bpy.ops.wm.usd_import(filepath=str(self.testdir / "usd_name_property_template.usda"), merge_parent_xform=True)
        prim_map = GetPrimMapUsdImportHook.prim_map
        bpy.utils.unregister_class(GetPrimMapUsdImportHook)

        expected_prim_map = {
            Sdf.Path('/Cube'): [bpy.data.objects["Cube.002"], bpy.data.meshes["Cube.002"]],
            Sdf.Path('/XformThenCube'): [bpy.data.objects["Cube"]],
            Sdf.Path('/XformThenCube/Cube'): [bpy.data.meshes["Cube"]],
            Sdf.Path('/XformThenXformCube'): [bpy.data.objects["XformThenXformCube"]],
            Sdf.Path('/XformThenXformCube/XformIntermediate'): [bpy.data.objects["Cube.001"]],
            Sdf.Path('/XformThenXformCube/XformIntermediate/Cube'): [bpy.data.meshes["Cube.001"]],
            Sdf.Path('/Material'): [bpy.data.materials["Material"]],
        }

        self.assertDictEqual(prim_map, expected_prim_map)

    def test_import_unit_scale(self):
        """Test importing a USD with 0.01 meters per unit."""

        infile = str(self.testdir / "usd_curve_bezier_all.usda")
        res = bpy.ops.wm.usd_import(filepath=infile, apply_unit_conversion_scale=True)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        # Ensure the root object was scaled by 0.01.
        root = bpy.data.objects["root"]
        self.assertEqual(self.round_vector(root.scale), [0.01, 0.01, 0.01])

        # Reimport with unit conversion scale off.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))

        infile = str(self.testdir / "usd_curve_bezier_all.usda")
        res = bpy.ops.wm.usd_import(filepath=infile, apply_unit_conversion_scale=False)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        # Ensure the root object has default scale 1.0.
        root = bpy.data.objects["root"]
        self.assertEqual(self.round_vector(root.scale), [1.0, 1.0, 1.0])

    def test_import_native_instancing(self):
        """Test importing USD files with scene instancing."""

        # Use the existing instancing test file to create the USD file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "nested_instancing_test.blend"))

        testfile = str(self.tempdir / "usd_export_nested_instancing_true.usda")
        res = bpy.ops.wm.usd_export(filepath=testfile, use_instancing=True)
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # If instancing is working there should only be 1 real mesh and 1 real point cloud
        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.pointclouds), 1)
        real_names = [bpy.data.meshes[0].name, bpy.data.pointclouds[0].name]

        # There should be 6 instances found for the above real objects (not counting empties etc.)
        instance_count = 0
        depsgraph = bpy.context.evaluated_depsgraph_get()
        for object_instance in depsgraph.object_instances:
            if object_instance.is_instance and object_instance.object.name in real_names:
                instance_count += 1

        self.assertEqual(instance_count, 6, "Unexpected number of instances found")

    def test_material_import_usd_hook(self):
        """Test importing color from an mtlx shader."""
        bpy.utils.register_class(ImportMtlxColorUSDHook)
        bpy.ops.wm.usd_import(filepath=str(self.testdir / "usd_simple_mtlx.usda"))
        bpy.utils.unregister_class(ImportMtlxColorUSDHook)

        # Check that the correct color was read.
        imported_color = ImportMtlxColorUSDHook.imported_color
        self.assertEqual(imported_color, [0, 1, 0, 1], "Wrong mtlx shader color imported")

        # Check that a Principled BSDF shader with the expected Base Color input.
        # was created.
        mtl = bpy.data.materials["Material"]
        bsdf = mtl.node_tree.nodes.get("Principled BSDF")
        self.assertIsNotNone(bsdf)
        base_color_input = bsdf.inputs['Base Color']
        self.assertEqual(self.round_vector(base_color_input.default_value), [0, 1, 0, 1])

    def test_usd_hook_import_texture(self):
        """
        Test importing a texture from a USDZ archive, using two different
        texture import modes.
        """

        bpy.utils.register_class(ImportMtlxTextureUSDHook)
        bpy.ops.wm.usd_import(filepath=str(self.testdir / "usd_simple_mtlx_texture.usdz"),
                              import_textures_mode='IMPORT_COPY',
                              import_textures_dir=str(self.tempdir / "textures"))

        # The resolved path should be a package-relative path for the USDZ, in this case
        # self.testdir / "usd_simple_mtlx_texture.usdz[textures/test_single.png]"
        resolved_path = ImportMtlxTextureUSDHook.resolved_path

        self.assertTrue(Ar.IsPackageRelativePath(resolved_path),
                        "Resolved path is not relative to the USDZ")
        path_inner = Ar.SplitPackageRelativePathInner(resolved_path)
        self.assertEqual(path_inner[1], "textures/test_single.png",
                         "Resolved path does not have the expected format")

        # Confirm that file was copied from the USDZ archive to the textures
        # directory.
        import_path = ImportMtlxTextureUSDHook.result[0]
        self.assertTrue(pathlib.Path(import_path).exists(),
                        "Imported texture does not exist")
        # Path should not be temporary
        is_temporary = ImportMtlxTextureUSDHook.result[1]
        self.assertFalse(is_temporary,
                         "Imported texture should not be temporary")

        # Repeat the test with texture packing enabled.
        bpy.ops.wm.usd_import(filepath=str(self.testdir / "usd_simple_mtlx_texture.usdz"),
                              import_textures_mode='IMPORT_PACK',
                              import_textures_dir="")

        # Confirm that the copied file exists.
        import_path = ImportMtlxTextureUSDHook.result[0]
        self.assertTrue(pathlib.Path(import_path).exists(),
                        "Imported texture does not exist")
        # Path should be temporary
        is_temporary = ImportMtlxTextureUSDHook.result[1]
        self.assertTrue(is_temporary,
                        "Imported texture should be temporary")

        bpy.utils.unregister_class(ImportMtlxTextureUSDHook)


class USDImportComparisonTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir
        cls.output_dir = args.outdir

    def test_import_usd(self):
        comparisondir = self.testdir.joinpath("compare")
        input_files = sorted(pathlib.Path(comparisondir).glob("*.usd*"))
        self.passed_tests = []
        self.failed_tests = []
        self.updated_tests = []

        from modules import io_report
        report = io_report.Report("USD Import", self.output_dir, comparisondir, comparisondir.joinpath("reference"))
        io_report.Report.context_lines = 8

        bpy.utils.register_class(CompareTestSupportHook)

        for input_file in input_files:
            input_file_path = pathlib.Path(input_file)

            io_report.Report.side_to_print_single_line = 5
            io_report.Report.side_to_print_multi_line = 3

            CompareTestSupportHook.reset_config()
            if input_file_path.name in ("nurbs-gen-single.usda", "nurbs-gen-multiple.usda", "nurbs-custom.usda"):
                CompareTestSupportHook.do_curve_rename = True
                io_report.Report.side_to_print_single_line = 10
                io_report.Report.side_to_print_multi_line = 10

            with self.subTest(input_file_path.stem):
                bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
                ok = report.import_and_check(
                    input_file, lambda filepath, params: bpy.ops.wm.usd_import(
                        filepath=str(input_file), import_subdivision=True, **params))
                if not ok:
                    self.fail(f"{input_file.stem} import result does not match expectations")

        bpy.utils.unregister_class(CompareTestSupportHook)
        report.finish("io_usd_import")


class CompareTestSupportHook(bpy.types.USDHook):
    bl_idname = "CompareTestSupportHook"
    bl_label = "Support some Comparison "

    do_curve_rename = False

    @staticmethod
    def reset_config():
        CompareTestSupportHook.do_curve_rename = False

    @staticmethod
    def on_import(context):
        prim_map = context.get_prim_map()

        if CompareTestSupportHook.do_curve_rename:
            for prim_path, objects in prim_map.items():
                if isinstance(objects[0], bpy.types.Object):
                    objects[0].name = prim_path.name
                elif isinstance(objects[0], bpy.types.Curves):
                    objects[0].name = prim_path.GetParentPath().name


class GetPrimMapUsdImportHook(bpy.types.USDHook):
    bl_idname = "get_prim_map_usd_import_hook"
    bl_label = "Get Prim Map Usd Import Hook"

    prim_map = None

    @staticmethod
    def on_import(context):
        GetPrimMapUsdImportHook.prim_map = context.get_prim_map()


class ImportMtlxColorUSDHook(bpy.types.USDHook):
    """
    Simple test for importing an mtxl shader from the usd_simple_mtlx.usda
    test file.
    """
    bl_idname = "import_mtlx_color_usd_hook"
    bl_label = "Import Mtlx Color USD Hook"

    imported_color = None

    @staticmethod
    def material_import_poll(import_context, usd_material):
        # We can import the material if it has an 'mtlx' context.
        surf_output = usd_material.GetSurfaceOutput("mtlx")
        return bool(surf_output)

    @staticmethod
    def on_material_import(import_context, bl_material, usd_material):

        # Get base_color from connected surface shader.
        surf_output = usd_material.GetSurfaceOutput("mtlx")
        assert surf_output
        source = surf_output.GetConnectedSource()
        assert source
        shader = UsdShade.Shader(source[0])
        assert shader
        color_attr = shader.GetInput("base_color")
        assert color_attr
        # Get the authored default color.
        color = color_attr.Get()

        # Add a Principled BSDF shader and set its 'Base Color' input to
        # the color we read from mtlx.
        node_tree = bl_material.node_tree
        assert node_tree
        nodes = node_tree.nodes
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        output = nodes.new("ShaderNodeOutputMaterial")
        node_tree.links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])
        color4 = [color[0], color[1], color[2], 1]
        ImportMtlxColorUSDHook.imported_color = color4
        bsdf.inputs['Base Color'].default_value = color4

        return True


class ImportMtlxTextureUSDHook(bpy.types.USDHook):
    """
    Simple test for importing a texture file from the
    usd_simple_mtlx_texture.usdz archive test file.
    """
    bl_idname = "import_mtlx_texture_usd_hook"
    bl_label = "Import Mtlx Texture USD Hook"

    resolved_path = None
    result = None

    @staticmethod
    def material_import_poll(import_context, usd_material):
        # We can import the material if it has an 'mtlx' context.
        surf_output = usd_material.GetSurfaceOutput("mtlx")
        return bool(surf_output)

    @staticmethod
    def on_material_import(import_context, bl_material, usd_material):
        # For the test, we get the texture file input of a shader known
        # to exist in the usd_simple_mtlx_texture.usdz archive.
        surf_output = usd_material.GetSurfaceOutput("mtlx")
        assert surf_output
        stage = import_context.get_stage()
        assert stage
        prim = stage.GetPrimAtPath("/root/_materials/Material/NodeGraphs/Image_Texture_Color")
        assert prim
        tex_node = UsdShade.Shader(prim)
        assert tex_node
        file_attr = tex_node.GetInput("file")
        assert file_attr
        file = file_attr.Get()

        # Record the file's resolved path, which should be a package-relative
        # path, e.g.,
        # c:/foo/bar/usd_simple_mtlx_texture.usdz[textures/test_single.png
        resolved_path = file.resolvedPath
        ImportMtlxTextureUSDHook.resolved_path = resolved_path

        result = import_context.import_texture(resolved_path)
        # Record the returned result tuple.  The first element of the tuple
        # is the texture path and the second is a flag indicating whether the
        # returned path references a temporary file.
        ImportMtlxTextureUSDHook.result = result

        return True


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

    unittest.main(argv=remaining, verbosity=0)


if __name__ == "__main__":
    main()
