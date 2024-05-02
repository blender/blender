# SPDX-FileCopyrightText: 2021-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import math
import pathlib
import sys
import unittest
import tempfile
from pxr import Usd
from pxr import UsdShade
from pxr import UsdGeom
from pxr import Sdf

import bpy

from mathutils import Matrix, Vector, Quaternion, Euler

args = None


class AbstractUSDTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir
        cls._tempdir = tempfile.TemporaryDirectory()
        cls.tempdir = pathlib.Path(cls._tempdir.name)

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        'Test dir {0} should exist'.format(self.testdir))

        # Make sure we always start with a known-empty file.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))

    def tearDown(self):
        self._tempdir.cleanup()


class USDImportTest(AbstractUSDTest):

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

        action = bpy.data.actions['SkelAction']

        # Verify the Elbow joint rotation animation.
        curve_path = 'pose.bones["Elbow"].rotation_quaternion'

        # Quat W
        f = action.fcurves.find(curve_path, index=0)
        self.assertIsNotNone(f, "Couldn't find Elbow rotation quaternion W curve")
        self.assertAlmostEqual(f.evaluate(0), 1.0, 2, "Unexpected value for rotation quaternion W curve at frame 0")
        self.assertAlmostEqual(f.evaluate(10), 0.707, 2, "Unexpected value for rotation quaternion W curve at frame 10")

        # Quat X
        f = action.fcurves.find(curve_path, index=1)
        self.assertIsNotNone(f, "Couldn't find Elbow rotation quaternion X curve")
        self.assertAlmostEqual(f.evaluate(0), 0.0, 2, "Unexpected value for rotation quaternion X curve at frame 0")
        self.assertAlmostEqual(f.evaluate(10), 0.707, 2, "Unexpected value for rotation quaternion X curve at frame 10")

        # Quat Y
        f = action.fcurves.find(curve_path, index=2)
        self.assertIsNotNone(f, "Couldn't find Elbow rotation quaternion Y curve")
        self.assertAlmostEqual(f.evaluate(0), 0.0, 2, "Unexpected value for rotation quaternion Y curve at frame 0")
        self.assertAlmostEqual(f.evaluate(10), 0.0, 2, "Unexpected value for rotation quaternion Y curve at frame 10")

        # Quat Z
        f = action.fcurves.find(curve_path, index=3)
        self.assertIsNotNone(f, "Couldn't find Elbow rotation quaternion Z curve")
        self.assertAlmostEqual(f.evaluate(0), 0.0, 2, "Unexpected value for rotation quaternion Z curve at frame 0")
        self.assertAlmostEqual(f.evaluate(10), 0.0, 2, "Unexpected value for rotation quaternion Z curve at frame 10")

    def check_curve(self, blender_curve, usd_curve):
        curve_type_map = {"linear": 1, "cubic": 2}
        cyclic_map = {"nonperiodic": False, "periodic": True}

        # Check correct spline count.
        blender_spline_count = len(blender_curve.attributes["curve_type"].data)
        usd_spline_count = len(usd_curve.GetCurveVertexCountsAttr().Get())
        self.assertEqual(blender_spline_count, usd_spline_count)

        # Check correct type of curve. All splines should have the same type and periodicity.
        usd_curve_type = usd_curve.GetTypeAttr().Get()
        usd_cyclic = usd_curve.GetWrapAttr().Get()
        expected_curve_type = curve_type_map[usd_curve_type]
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
        if usd_curve_type == "linear":
            point_count = len(usd_positions)
            self.assertEqual(len(blender_positions), point_count)
        elif usd_curve_type == "cubic":
            control_point_count = 0
            usd_vert_counts = usd_curve.GetCurveVertexCountsAttr().Get()
            for i in range(0, usd_spline_count):
                if usd_cyclic == "nonperiodic":
                    control_point_count += (int(usd_vert_counts[i] / 3) + 1)
                else:
                    control_point_count += (int(usd_vert_counts[i] / 3))

            point_count = control_point_count
            self.assertEqual(len(blender_positions), point_count)

        # Check radius data.
        usd_width_interpolation = usd_curve.GetWidthsInterpolation()
        usd_radius = [w / 2 for w in usd_curve.GetWidthsAttr().Get()]
        blender_radius = [r.value for r in blender_curve.attributes["radius"].data]
        if usd_curve_type == "linear":
            if usd_width_interpolation == "constant":
                usd_radius = usd_radius * point_count

            for i in range(0, len(blender_radius)):
                self.assertAlmostEqual(blender_radius[i], usd_radius[i], 2)

        elif usd_curve_type == "cubic":
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

    def test_import_light_types(self):
        """Test importing light types and attributes."""

        # Use the current scene to first create and export the lights
        bpy.ops.object.light_add(type='POINT', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        bpy.context.active_object.data.energy = 2
        bpy.context.active_object.data.shadow_soft_size = 2.2

        bpy.ops.object.light_add(type='SPOT', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        bpy.context.active_object.data.energy = 3
        bpy.context.active_object.data.shadow_soft_size = 3.3
        bpy.context.active_object.data.spot_blend = 0.25
        bpy.context.active_object.data.spot_size = math.radians(60)

        bpy.ops.object.light_add(type='SUN', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        bpy.context.active_object.data.energy = 4
        bpy.context.active_object.data.angle = math.radians(1)

        bpy.ops.object.light_add(type='AREA', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        bpy.context.active_object.data.energy = 5
        bpy.context.active_object.data.shape = 'RECTANGLE'
        bpy.context.active_object.data.size = 0.5
        bpy.context.active_object.data.size_y = 1.5

        bpy.ops.object.light_add(type='AREA', align='WORLD', location=(0, 0, 0), scale=(1, 1, 1))
        bpy.context.active_object.data.energy = 6
        bpy.context.active_object.data.shape = 'DISK'
        bpy.context.active_object.data.size = 2

        test_path = self.tempdir / "temp_lights.usda"
        res = bpy.ops.wm.usd_export(filepath=str(test_path), evaluation_mode="RENDER")
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {test_path}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        infile = str(test_path)
        res = bpy.ops.wm.usd_import(filepath=infile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {infile}")

        lights = [o for o in bpy.data.objects if o.type == 'LIGHT']
        self.assertEqual(5, len(lights), f"Test scene {infile} should have 5 lights; found {len(lights)}")

        blender_light = bpy.data.lights["Point"]
        self.assertAlmostEqual(blender_light.energy, 2, 3)
        self.assertAlmostEqual(blender_light.shadow_soft_size, 2.2, 3)

        blender_light = bpy.data.lights["Spot"]
        self.assertAlmostEqual(blender_light.energy, 3, 3)
        self.assertAlmostEqual(blender_light.shadow_soft_size, 3.3, 3)
        self.assertAlmostEqual(blender_light.spot_blend, 0.25, 3)
        self.assertAlmostEqual(blender_light.spot_size, math.radians(60), 3)

        blender_light = bpy.data.lights["Sun"]
        self.assertAlmostEqual(blender_light.energy, 4, 3)
        self.assertAlmostEqual(blender_light.angle, math.radians(1), 3)

        blender_light = bpy.data.lights["Area"]
        self.assertAlmostEqual(blender_light.energy, 5, 3)
        self.assertEqual(blender_light.shape, 'RECTANGLE')
        self.assertAlmostEqual(blender_light.size, 0.5, 3)
        self.assertAlmostEqual(blender_light.size_y, 1.5, 3)

        blender_light = bpy.data.lights["Area_001"]
        self.assertAlmostEqual(blender_light.energy, 6, 3)
        self.assertEqual(blender_light.shape, 'DISK')
        self.assertAlmostEqual(blender_light.size, 2, 3)

    def check_attribute(self, blender_data, attribute_name, domain, data_type, elements_len):
        attr = blender_data.attributes[attribute_name]
        self.assertEqual(attr.domain, domain)
        self.assertEqual(attr.data_type, data_type)
        self.assertEqual(len(attr.data), elements_len)

    def check_attribute_missing(self, blender_data, attribute_name):
        self.assertFalse(attribute_name in blender_data.attributes)

    def test_import_attributes(self):
        testfile = str(self.tempdir / "usd_attribute_test.usda")

        # Use the existing attributes file to create the USD test file
        # for import. It is validated as part of the bl_usd_export test.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_attribute_test.blend"))
        res = bpy.ops.wm.usd_export(filepath=testfile, evaluation_mode="RENDER")
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {testfile}")

        # Reload the empty file and import back in
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        res = bpy.ops.wm.usd_import(filepath=testfile)
        self.assertEqual({'FINISHED'}, res, f"Unable to import USD file {testfile}")

        # Verify all attributes on the Mesh
        # Note: USD does not support signed 8-bit types so there is
        #       currently no equivalent to Blender's INT8 data type
        # TODO: Blender is missing support for reading USD quat/matrix data types
        mesh = bpy.data.objects["Mesh"].data

        self.check_attribute(mesh, "p_bool", 'POINT', 'BOOLEAN', 4)
        self.check_attribute(mesh, "p_int8", 'POINT', 'INT', 4)
        self.check_attribute(mesh, "p_int32", 'POINT', 'INT', 4)
        self.check_attribute(mesh, "p_float", 'POINT', 'FLOAT', 4)
        self.check_attribute(mesh, "p_byte_color", 'POINT', 'FLOAT_COLOR', 4)
        self.check_attribute(mesh, "p_color", 'POINT', 'FLOAT_COLOR', 4)
        self.check_attribute(mesh, "p_vec2", 'CORNER', 'FLOAT2', 4)  # TODO: Bug - wrong domain
        self.check_attribute(mesh, "p_vec3", 'POINT', 'FLOAT_VECTOR', 4)
        self.check_attribute_missing(mesh, "p_quat")
        self.check_attribute_missing(mesh, "p_mat4x4")

        self.check_attribute(mesh, "f_bool", 'FACE', 'BOOLEAN', 1)
        self.check_attribute(mesh, "f_int8", 'FACE', 'INT', 1)
        self.check_attribute(mesh, "f_int32", 'FACE', 'INT', 1)
        self.check_attribute(mesh, "f_float", 'FACE', 'FLOAT', 1)
        self.check_attribute_missing(mesh, "f_byte_color")  # Not supported?
        self.check_attribute_missing(mesh, "f_color")  # Not supported?
        self.check_attribute(mesh, "f_vec2", 'FACE', 'FLOAT2', 1)
        self.check_attribute(mesh, "f_vec3", 'FACE', 'FLOAT_VECTOR', 1)
        self.check_attribute_missing(mesh, "f_quat")
        self.check_attribute_missing(mesh, "f_mat4x4")

        self.check_attribute(mesh, "fc_bool", 'CORNER', 'BOOLEAN', 4)
        self.check_attribute(mesh, "fc_int8", 'CORNER', 'INT', 4)
        self.check_attribute(mesh, "fc_int32", 'CORNER', 'INT', 4)
        self.check_attribute(mesh, "fc_float", 'CORNER', 'FLOAT', 4)
        self.check_attribute(mesh, "fc_byte_color", 'CORNER', 'FLOAT_COLOR', 4)
        self.check_attribute(mesh, "fc_color", 'CORNER', 'FLOAT_COLOR', 4)
        self.check_attribute(mesh, "fc_vec2", 'CORNER', 'FLOAT2', 4)
        self.check_attribute(mesh, "fc_vec3", 'CORNER', 'FLOAT_VECTOR', 4)
        self.check_attribute_missing(mesh, "fc_quat")
        self.check_attribute_missing(mesh, "fc_mat4x4")


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
