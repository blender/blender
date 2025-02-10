# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import math
import os
import pathlib
import pprint
import sys
import tempfile
import unittest
from pxr import Gf, Sdf, Usd, UsdGeom, UsdShade, UsdSkel, UsdUtils, UsdVol

import bpy

sys.path.append(str(pathlib.Path(__file__).parent.absolute()))
from modules.colored_print import (print_message, use_message_colors)


args = None


class AbstractUSDTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tempdir = tempfile.TemporaryDirectory()
        cls.testdir = args.testdir
        cls.tempdir = pathlib.Path(cls._tempdir.name)
        if os.environ.get("BLENDER_TEST_COLOR") is not None:
            use_message_colors()

    def setUp(self):
        self.assertTrue(self.testdir.exists(), "Test dir {0} should exist".format(self.testdir))
        print_message(self._testMethodName, 'SUCCESS', 'RUN')

    def tearDown(self):
        self._tempdir.cleanup()

        result = self._outcome.result
        ok = all(test != self for test, _ in result.errors + result.failures)
        if not ok:
            print_message(self._testMethodName, 'FAILURE', 'FAILED')
        else:
            print_message(self._testMethodName, 'SUCCESS', 'PASSED')

    def export_and_validate(self, **kwargs):
        """Export and validate the resulting USD file."""

        export_path = kwargs["filepath"]

        # Do the actual export
        res = bpy.ops.wm.usd_export(**kwargs)
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {export_path}")

        # Validate resulting file
        checker = UsdUtils.ComplianceChecker(
            arkit=False,
            skipARKitRootLayerCheck=False,
            rootPackageOnly=False,
            skipVariants=False,
            verbose=False,
        )
        checker.CheckCompliance(export_path)

        failed_checks = {}

        # The ComplianceChecker does not know how to resolve <UDIM> tags, so
        # it will flag "textures/test_grid_<UDIM>.png" as a missing reference.
        # That reference is in fact OK, so we skip the rule for this test.
        to_skip = ("MissingReferenceChecker",)
        for rule in checker._rules:
            name = rule.__class__.__name__
            if name in to_skip:
                continue

            issues = rule.GetFailedChecks() + rule.GetWarnings() + rule.GetErrors()
            if not issues:
                continue

            failed_checks[name] = issues

        self.assertFalse(failed_checks, pprint.pformat(failed_checks))


class USDExportTest(AbstractUSDTest):
    # Utility function to round each component of a vector to a few digits. The "+ 0" is to
    # ensure that any negative zeros (-0.0) are converted to positive zeros (0.0).
    @staticmethod
    def round_vector(vector):
        return [round(c, 4) + 0 for c in vector]

    # Utility function to compare two Gf.Vec3d's
    def compareVec3d(self, first, second):
        places = 5
        self.assertAlmostEqual(first[0], second[0], places)
        self.assertAlmostEqual(first[1], second[1], places)
        self.assertAlmostEqual(first[2], second[2], places)

    def test_export_extents(self):
        """Test that exported scenes contain have a properly authored extent attribute on each boundable prim"""
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_extent_test.blend"))
        export_path = self.tempdir / "usd_extent_test.usda"

        self.export_and_validate(
            filepath=str(export_path),
            export_materials=True,
            evaluation_mode="RENDER",
            convert_world_material=False,
        )

        # if prims are missing, the exporter must have skipped some objects
        stats = UsdUtils.ComputeUsdStageStats(str(export_path))
        self.assertEqual(stats["totalPrimCount"], 16, "Unexpected number of prims")

        # validate the overall world bounds of the scene
        stage = Usd.Stage.Open(str(export_path))
        scenePrim = stage.GetPrimAtPath("/root/scene")
        bboxcache = UsdGeom.BBoxCache(Usd.TimeCode.Default(), [UsdGeom.Tokens.default_])
        bounds = bboxcache.ComputeWorldBound(scenePrim)
        bound_min = bounds.GetRange().GetMin()
        bound_max = bounds.GetRange().GetMax()
        self.compareVec3d(bound_min, Gf.Vec3d(-5.752975881, -1, -2.798513651))
        self.compareVec3d(bound_max, Gf.Vec3d(1, 2.9515805244, 2.7985136508))

        # validate the locally authored extents
        prim = stage.GetPrimAtPath("/root/scene/BigCube/BigCubeMesh")
        extent = UsdGeom.Boundable(prim).GetExtentAttr().Get()
        self.compareVec3d(Gf.Vec3d(extent[0]), Gf.Vec3d(-1, -1, -2.7985137))
        self.compareVec3d(Gf.Vec3d(extent[1]), Gf.Vec3d(1, 1, 2.7985137))
        prim = stage.GetPrimAtPath("/root/scene/LittleCube/LittleCubeMesh")
        extent = UsdGeom.Boundable(prim).GetExtentAttr().Get()
        self.compareVec3d(Gf.Vec3d(extent[0]), Gf.Vec3d(-1, -1, -1))
        self.compareVec3d(Gf.Vec3d(extent[1]), Gf.Vec3d(1, 1, 1))
        prim = stage.GetPrimAtPath("/root/scene/Volume/Volume")
        extent = UsdGeom.Boundable(prim).GetExtentAttr().Get()
        self.compareVec3d(
            Gf.Vec3d(extent[0]), Gf.Vec3d(-0.7313742, -0.68043584, -0.5801515)
        )
        self.compareVec3d(
            Gf.Vec3d(extent[1]), Gf.Vec3d(0.7515701, 0.5500924, 0.9027928)
        )

    def test_material_transforms(self):
        """Validate correct export of image mapping parameters to the UsdTransform2d shader def"""

        # Use the common materials .blend file
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_export.blend"))
        export_path = self.tempdir / "material_transforms.usda"
        self.export_and_validate(filepath=str(export_path), export_materials=True)

        # Inspect the UsdTransform2d prim on the "Transforms" material
        stage = Usd.Stage.Open(str(export_path))
        shader_prim = stage.GetPrimAtPath("/root/_materials/Transforms/Mapping")
        shader = UsdShade.Shader(shader_prim)
        self.assertEqual(shader.GetIdAttr().Get(), "UsdTransform2d")
        input_trans = shader.GetInput('translation')
        input_rot = shader.GetInput('rotation')
        input_scale = shader.GetInput('scale')
        self.assertEqual(input_trans.Get(), [0.75, 0.75])
        self.assertEqual(input_rot.Get(), 180)
        self.assertEqual(input_scale.Get(), [0.5, 0.5])

    def test_material_normal_maps(self):
        """Validate correct export of typical normal map setups to the UsdUVTexture shader def.
        Namely validate that scale, bias, and ColorSpace settings are correct"""

        # Use the common materials .blend file
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_export.blend"))
        export_path = self.tempdir / "material_normalmaps.usda"
        self.export_and_validate(filepath=str(export_path), export_materials=True)

        # Inspect the UsdUVTexture prim on the "typical" "NormalMap" material
        stage = Usd.Stage.Open(str(export_path))
        shader_prim = stage.GetPrimAtPath("/root/_materials/NormalMap/Image_Texture")
        shader = UsdShade.Shader(shader_prim)
        self.assertEqual(shader.GetIdAttr().Get(), "UsdUVTexture")
        input_scale = shader.GetInput('scale')
        input_bias = shader.GetInput('bias')
        input_colorspace = shader.GetInput('sourceColorSpace')
        self.assertEqual(input_scale.Get(), [2, 2, 2, 2])
        self.assertEqual(input_bias.Get(), [-1, -1, -1, -1])
        self.assertEqual(input_colorspace.Get(), 'raw')

        # Inspect the UsdUVTexture prim on the "inverted" "NormalMap_Scale_Bias" material
        stage = Usd.Stage.Open(str(export_path))
        shader_prim = stage.GetPrimAtPath("/root/_materials/NormalMap_Scale_Bias/Image_Texture")
        shader = UsdShade.Shader(shader_prim)
        self.assertEqual(shader.GetIdAttr().Get(), "UsdUVTexture")
        input_scale = shader.GetInput('scale')
        input_bias = shader.GetInput('bias')
        input_colorspace = shader.GetInput('sourceColorSpace')
        self.assertEqual(input_scale.Get(), [2, -2, 2, 1])
        self.assertEqual(input_bias.Get(), [-1, 1, -1, 0])
        self.assertEqual(input_colorspace.Get(), 'raw')

    def test_material_opacity_threshold(self):
        """Validate correct export of opacity and opacity_threshold parameters to the UsdPreviewSurface shader def"""

        # Use the common materials .blend file
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_channels.blend"))
        export_path = self.tempdir / "usd_materials_channels.usda"
        self.export_and_validate(filepath=str(export_path), export_materials=True)

        # Opaque no-Alpha
        stage = Usd.Stage.Open(str(export_path))
        shader_prim = stage.GetPrimAtPath("/root/_materials/Opaque/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        opacity_input = shader.GetInput('opacity')
        self.assertEqual(opacity_input.HasConnectedSource(), False,
                         "Opacity input should not be connected for opaque material")
        self.assertAlmostEqual(opacity_input.Get(), 1.0, 2, "Opacity input should be set to 1")

        # Validate Image Alpha to BSDF Alpha
        shader_prim = stage.GetPrimAtPath("/root/_materials/Alpha/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        opacity_input = shader.GetInput('opacity')
        opacity_thresh_input = shader.GetInput('opacityThreshold')
        self.assertEqual(opacity_input.HasConnectedSource(), True, "Alpha input should be connected")
        self.assertEqual(opacity_thresh_input.Get(), None, "Opacity threshold input should be empty")

        # Validate Image Alpha to BSDF Alpha w/Round
        shader_prim = stage.GetPrimAtPath("/root/_materials/AlphaClip_Round/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        opacity_input = shader.GetInput('opacity')
        opacity_thresh_input = shader.GetInput('opacityThreshold')
        self.assertEqual(opacity_input.HasConnectedSource(), True, "Alpha input should be connected")
        self.assertAlmostEqual(opacity_thresh_input.Get(), 0.5, 2, "Opacity threshold input should be 0.5")

        # Validate Image Alpha to BSDF Alpha w/LessThan+Invert
        shader_prim = stage.GetPrimAtPath("/root/_materials/AlphaClip_LessThan/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        opacity_input = shader.GetInput('opacity')
        opacity_thresh_input = shader.GetInput('opacityThreshold')
        self.assertEqual(opacity_input.HasConnectedSource(), True, "Alpha input should be connected")
        self.assertAlmostEqual(opacity_thresh_input.Get(), 0.8, 2, "Opacity threshold input should be 0.8")

        # Validate Image RGB to BSDF Metallic, Roughness, Alpha
        shader_prim = stage.GetPrimAtPath("/root/_materials/Channel/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        metallic_input = shader.GetInput("metallic")
        roughness_input = shader.GetInput("roughness")
        opacity_input = shader.GetInput('opacity')
        opacity_thresh_input = shader.GetInput('opacityThreshold')
        self.assertEqual(metallic_input.HasConnectedSource(), True, "Metallic input should be connected")
        self.assertEqual(roughness_input.HasConnectedSource(), True, "Roughness input should be connected")
        self.assertEqual(opacity_input.HasConnectedSource(), True, "Alpha input should be connected")
        self.assertEqual(opacity_thresh_input.Get(), None, "Opacity threshold input should be empty")

        # Validate Image RGB to BSDF Metallic, Roughness, Alpha w/Round
        shader_prim = stage.GetPrimAtPath("/root/_materials/ChannelClip_Round/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        metallic_input = shader.GetInput("metallic")
        roughness_input = shader.GetInput("roughness")
        opacity_input = shader.GetInput('opacity')
        opacity_thresh_input = shader.GetInput('opacityThreshold')
        self.assertEqual(metallic_input.HasConnectedSource(), True, "Metallic input should be connected")
        self.assertEqual(roughness_input.HasConnectedSource(), True, "Roughness input should be connected")
        self.assertEqual(opacity_input.HasConnectedSource(), True, "Alpha input should be connected")
        self.assertAlmostEqual(opacity_thresh_input.Get(), 0.5, 2, "Opacity threshold input should be 0.5")

        # Validate Image RGB to BSDF Metallic, Roughness, Alpha w/LessThan+Invert
        shader_prim = stage.GetPrimAtPath("/root/_materials/ChannelClip_LessThan/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        metallic_input = shader.GetInput("metallic")
        roughness_input = shader.GetInput("roughness")
        opacity_input = shader.GetInput('opacity')
        opacity_thresh_input = shader.GetInput('opacityThreshold')
        self.assertEqual(metallic_input.HasConnectedSource(), True, "Metallic input should be connected")
        self.assertEqual(roughness_input.HasConnectedSource(), True, "Roughness input should be connected")
        self.assertEqual(opacity_input.HasConnectedSource(), True, "Alpha input should be connected")
        self.assertAlmostEqual(opacity_thresh_input.Get(), 0.2, 2, "Opacity threshold input should be 0.2")

    def test_export_material_subsets(self):
        """Validate multiple materials assigned to the same mesh work correctly."""

        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_multi.blend"))

        # Ensure the simulation zone data is baked for all relevant frames...
        for frame in range(1, 5):
            bpy.context.scene.frame_set(frame)
        bpy.context.scene.frame_set(1)

        export_path = self.tempdir / "usd_materials_multi.usda"
        self.export_and_validate(filepath=str(export_path), export_animation=True, evaluation_mode="RENDER")

        stage = Usd.Stage.Open(str(export_path))

        # The static mesh should have 4 materials each assigned to 4 faces (16 faces total)
        static_mesh_prim = UsdGeom.Mesh(stage.GetPrimAtPath("/root/static_mesh/static_mesh"))
        geom_subsets = UsdGeom.Subset.GetGeomSubsets(static_mesh_prim)
        self.assertEqual(len(geom_subsets), 4)

        unique_face_indices = set()
        for subset in geom_subsets:
            face_indices = subset.GetIndicesAttr().Get()
            self.assertEqual(len(face_indices), 4)
            unique_face_indices.update(face_indices)
        self.assertEqual(len(unique_face_indices), 16)

        # The dynamic mesh varies over time (currently blocked, see #124554 and #118754)
        #  - Frame 1: 1 face and 1 material [mat2]
        #  - Frame 2: 2 faces and 2 materials [mat2, mat3]
        #  - Frame 3: 4 faces and 3 materials [mat2, mat3, mat2, mat1]
        #  - Frame 4: 4 faces and 2 materials [mat2, mat3, mat2, mat3]
        dynamic_mesh_prim = UsdGeom.Mesh(stage.GetPrimAtPath("/root/dynamic_mesh/dynamic_mesh"))
        geom_subsets = UsdGeom.Subset.GetGeomSubsets(dynamic_mesh_prim)
        self.assertEqual(len(geom_subsets), 0)

    def test_export_material_inmem(self):
        """Validate correct export of in memory and packed images"""

        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_inmem_pack.blend"))
        export_path1 = self.tempdir / "usd_materials_inmem_pack_relative.usda"
        self.export_and_validate(filepath=str(export_path1), export_textures_mode='NEW', relative_paths=True)

        export_path2 = self.tempdir / "usd_materials_inmem_pack_absolute.usda"
        self.export_and_validate(filepath=str(export_path2), export_textures_mode='NEW', relative_paths=False)

        # Validate that we actually see the correct set of files being saved to the filesystem

        # Relative path variations
        stage = Usd.Stage.Open(str(export_path1))
        stage_path = pathlib.Path(stage.GetRootLayer().realPath)

        shader_prim = stage.GetPrimAtPath("/root/_materials/MAT_inmem_single/Image_Texture")
        shader = UsdShade.Shader(shader_prim)
        asset_path = pathlib.Path(shader.GetInput("file").GetAttr().Get().path)
        self.assertFalse(asset_path.is_absolute())
        self.assertTrue(stage_path.parent.joinpath(asset_path).is_file())

        shader_prim = stage.GetPrimAtPath("/root/_materials/MAT_inmem_udim/Image_Texture")
        shader = UsdShade.Shader(shader_prim)
        asset_path = pathlib.Path(shader.GetInput("file").GetAttr().Get().path)
        image_path1 = pathlib.Path(str(asset_path).replace("<UDIM>", "1001"))
        image_path2 = pathlib.Path(str(asset_path).replace("<UDIM>", "1002"))
        self.assertFalse(asset_path.is_absolute())
        self.assertTrue(stage_path.parent.joinpath(image_path1).is_file())
        self.assertTrue(stage_path.parent.joinpath(image_path2).is_file())

        shader_prim = stage.GetPrimAtPath("/root/_materials/MAT_pack_single/Image_Texture")
        shader = UsdShade.Shader(shader_prim)
        asset_path = pathlib.Path(shader.GetInput("file").GetAttr().Get().path)
        self.assertFalse(asset_path.is_absolute())
        self.assertTrue(stage_path.parent.joinpath(asset_path).is_file())

        shader_prim = stage.GetPrimAtPath("/root/_materials/MAT_pack_udim/Image_Texture")
        shader = UsdShade.Shader(shader_prim)
        asset_path = pathlib.Path(shader.GetInput("file").GetAttr().Get().path)
        image_path1 = pathlib.Path(str(asset_path).replace("<UDIM>", "1001"))
        image_path2 = pathlib.Path(str(asset_path).replace("<UDIM>", "1002"))
        self.assertFalse(asset_path.is_absolute())
        self.assertTrue(stage_path.parent.joinpath(image_path1).is_file())
        self.assertTrue(stage_path.parent.joinpath(image_path2).is_file())

        # Absolute path variations
        stage = Usd.Stage.Open(str(export_path2))
        stage_path = pathlib.Path(stage.GetRootLayer().realPath)

        shader_prim = stage.GetPrimAtPath("/root/_materials/MAT_inmem_single/Image_Texture")
        shader = UsdShade.Shader(shader_prim)
        asset_path = pathlib.Path(shader.GetInput("file").GetAttr().Get().path)
        self.assertTrue(asset_path.is_absolute())
        self.assertTrue(stage_path.parent.joinpath(asset_path).is_file())

        shader_prim = stage.GetPrimAtPath("/root/_materials/MAT_inmem_udim/Image_Texture")
        shader = UsdShade.Shader(shader_prim)
        asset_path = pathlib.Path(shader.GetInput("file").GetAttr().Get().path)
        image_path1 = pathlib.Path(str(asset_path).replace("<UDIM>", "1001"))
        image_path2 = pathlib.Path(str(asset_path).replace("<UDIM>", "1002"))
        self.assertTrue(asset_path.is_absolute())
        self.assertTrue(stage_path.parent.joinpath(image_path1).is_file())
        self.assertTrue(stage_path.parent.joinpath(image_path2).is_file())

        shader_prim = stage.GetPrimAtPath("/root/_materials/MAT_pack_single/Image_Texture")
        shader = UsdShade.Shader(shader_prim)
        asset_path = pathlib.Path(shader.GetInput("file").GetAttr().Get().path)
        self.assertTrue(asset_path.is_absolute())
        self.assertTrue(stage_path.parent.joinpath(asset_path).is_file())

        shader_prim = stage.GetPrimAtPath("/root/_materials/MAT_pack_udim/Image_Texture")
        shader = UsdShade.Shader(shader_prim)
        asset_path = pathlib.Path(shader.GetInput("file").GetAttr().Get().path)
        image_path1 = pathlib.Path(str(asset_path).replace("<UDIM>", "1001"))
        image_path2 = pathlib.Path(str(asset_path).replace("<UDIM>", "1002"))
        self.assertTrue(asset_path.is_absolute())
        self.assertTrue(stage_path.parent.joinpath(image_path1).is_file())
        self.assertTrue(stage_path.parent.joinpath(image_path2).is_file())

    def test_export_material_textures_mode(self):
        """Validate the non-default export textures mode options."""

        # Use the common materials .blend file
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_export.blend"))

        # For this test, the "textures" directory should NOT exist and the image paths
        # should all point to the original test location, not the temp output location.
        def check_image_paths(stage):
            orig_tex_path = (self.testdir / "textures")
            temp_tex_path = (self.tempdir / "textures")
            self.assertFalse(temp_tex_path.is_dir())

            shader_prim = stage.GetPrimAtPath("/root/_materials/Material/Image_Texture")
            shader = UsdShade.Shader(shader_prim)
            filepath = pathlib.Path(shader.GetInput('file').Get().path)
            self.assertEqual(orig_tex_path, filepath.parent)

        export_file = str(self.tempdir / "usd_materials_texture_preserve.usda")
        self.export_and_validate(
            filepath=export_file, export_materials=True, convert_world_material=False, export_textures_mode='PRESERVE')
        check_image_paths(Usd.Stage.Open(export_file))

        export_file = str(self.tempdir / "usd_materials_texture_keep.usda")
        self.export_and_validate(
            filepath=export_file, export_materials=True, convert_world_material=False, export_textures_mode='KEEP')
        check_image_paths(Usd.Stage.Open(export_file))

    def test_export_material_displacement(self):
        """Validate correct export of Displacement information for the UsdPreviewSurface"""

        # Use the common materials .blend file
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_displace.blend"))
        export_path = self.tempdir / "material_displace.usda"
        self.export_and_validate(filepath=str(export_path), export_materials=True)

        stage = Usd.Stage.Open(str(export_path))

        # Verify "constant" displacement
        shader_surface = UsdShade.Shader(stage.GetPrimAtPath("/root/_materials/constant/Principled_BSDF"))
        self.assertEqual(shader_surface.GetIdAttr().Get(), "UsdPreviewSurface")
        input_displacement = shader_surface.GetInput('displacement')
        self.assertEqual(input_displacement.HasConnectedSource(), False, "Displacement input should not be connected")
        self.assertAlmostEqual(input_displacement.Get(), 0.45, 5)

        # Validate various Midlevel and Scale scenarios
        def validate_displacement(mat_name, expected_scale, expected_bias):
            shader_surface = UsdShade.Shader(stage.GetPrimAtPath(f"/root/_materials/{mat_name}/Principled_BSDF"))
            shader_image = UsdShade.Shader(stage.GetPrimAtPath(f"/root/_materials/{mat_name}/Image_Texture"))
            self.assertEqual(shader_surface.GetIdAttr().Get(), "UsdPreviewSurface")
            self.assertEqual(shader_image.GetIdAttr().Get(), "UsdUVTexture")
            input_displacement = shader_surface.GetInput('displacement')
            input_colorspace = shader_image.GetInput('sourceColorSpace')
            input_scale = shader_image.GetInput('scale')
            input_bias = shader_image.GetInput('bias')
            self.assertEqual(input_displacement.HasConnectedSource(), True, "Displacement input should be connected")
            self.assertEqual(input_colorspace.Get(), 'raw')
            self.assertEqual(self.round_vector(input_scale.Get()), expected_scale)
            self.assertEqual(self.round_vector(input_bias.Get()), expected_bias)

        validate_displacement("mid_0_0", [1.0, 1.0, 1.0, 1.0], [0, 0, 0, 0])
        validate_displacement("mid_0_5", [1.0, 1.0, 1.0, 1.0], [-0.5, -0.5, -0.5, 0])
        validate_displacement("mid_1_0", [1.0, 1.0, 1.0, 1.0], [-1, -1, -1, 0])
        validate_displacement("mid_0_0_scale_0_3", [0.3, 0.3, 0.3, 1.0], [0, 0, 0, 0])
        validate_displacement("mid_0_5_scale_0_3", [0.3, 0.3, 0.3, 1.0], [-0.15, -0.15, -0.15, 0])
        validate_displacement("mid_1_0_scale_0_3", [0.3, 0.3, 0.3, 1.0], [-0.3, -0.3, -0.3, 0])

        # Validate that no displacement occurs for scenarios USD doesn't support
        shader_surface = UsdShade.Shader(stage.GetPrimAtPath(f"/root/_materials/bad_wrong_space/Principled_BSDF"))
        input_displacement = shader_surface.GetInput('displacement')
        self.assertTrue(input_displacement.Get() is None)
        shader_surface = UsdShade.Shader(stage.GetPrimAtPath(f"/root/_materials/bad_non_const/Principled_BSDF"))
        input_displacement = shader_surface.GetInput('displacement')
        self.assertTrue(input_displacement.Get() is None)

    def test_export_metaballs(self):
        """Validate correct export of Metaball objects. These are written out as Meshes."""

        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_metaballs.blend"))
        export_path = self.tempdir / "usd_metaballs.usda"
        self.export_and_validate(filepath=str(export_path), evaluation_mode="RENDER")

        stage = Usd.Stage.Open(str(export_path))

        # There should be 3 Mesh prims and they should each correspond to the "basis"
        # metaball (i.e. the ones without any numeric suffix)
        mesh_prims = [prim for prim in stage.Traverse() if prim.IsA(UsdGeom.Mesh)]
        prim_names = [prim.GetPath().pathString for prim in mesh_prims]
        self.assertEqual(len(mesh_prims), 3)
        self.assertListEqual(
            sorted(prim_names), ["/root/Ball_A/Ball_A", "/root/Ball_B/Ball_B", "/root/Ball_C/Ball_C"])

        # Make rough check of vertex counts to ensure geometry is present
        actual_prim_verts = {prim.GetName(): len(UsdGeom.Mesh(prim).GetPointsAttr().Get()) for prim in mesh_prims}
        expected_prim_verts = {"Ball_A": 2232, "Ball_B": 2876, "Ball_C": 1152}
        self.assertDictEqual(actual_prim_verts, expected_prim_verts)

    def test_particle_hair(self):
        """Validate correct export of particle hair emitters."""

        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_particle_hair.blend"))

        # Ensure the hair dynamics are baked for all relevant frames...
        for frame in range(1, 11):
            bpy.context.scene.frame_set(frame)
        bpy.context.scene.frame_set(1)

        export_path = self.tempdir / "usd_particle_hair.usda"
        self.export_and_validate(
            filepath=str(export_path), export_hair=True, export_animation=True, evaluation_mode="RENDER")

        stage = Usd.Stage.Open(str(export_path))
        main_prim = stage.GetPrimAtPath("/root/Sphere")
        hair_prim = stage.GetPrimAtPath("/root/Sphere/ParticleSystem")
        self.assertTrue(main_prim.IsValid())
        self.assertTrue(hair_prim.IsValid())

        # Ensure we have 5 frames of rotation data for the main Sphere and 10 frames for the hair data
        rot_samples = UsdGeom.Xformable(main_prim).GetRotateXYZOp().GetTimeSamples()
        self.assertEqual(len(rot_samples), 5)

        hair_curves = UsdGeom.BasisCurves(hair_prim)
        hair_samples = hair_curves.GetPointsAttr().GetTimeSamples()
        self.assertEqual(hair_curves.GetTypeAttr().Get(), "cubic")
        self.assertEqual(hair_curves.GetBasisAttr().Get(), "bspline")
        self.assertEqual(len(hair_samples), 10)

    def check_primvar(self, prim, pv_name, pv_typeName, pv_interp, elements_len):
        pv = UsdGeom.PrimvarsAPI(prim).GetPrimvar(pv_name)
        self.assertTrue(pv.HasValue())
        self.assertEqual(pv.GetTypeName().type.typeName, pv_typeName)
        self.assertEqual(pv.GetInterpolation(), pv_interp)
        self.assertEqual(len(pv.Get()), elements_len)

    def check_primvar_missing(self, prim, pv_name):
        pv = UsdGeom.PrimvarsAPI(prim).GetPrimvar(pv_name)
        self.assertFalse(pv.HasValue())

    def test_export_attributes(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_attribute_test.blend"))
        export_path = self.tempdir / "usd_attribute_test.usda"
        self.export_and_validate(filepath=str(export_path), evaluation_mode="RENDER")

        stage = Usd.Stage.Open(str(export_path))

        # Validate all expected Mesh attributes. Notice that nothing on
        # the Edge domain is supported by USD.
        prim = stage.GetPrimAtPath("/root/Mesh/Mesh")

        self.check_primvar(prim, "p_bool", "VtArray<bool>", "vertex", 4)
        self.check_primvar(prim, "p_int8", "VtArray<unsigned char>", "vertex", 4)
        self.check_primvar(prim, "p_int32", "VtArray<int>", "vertex", 4)
        self.check_primvar(prim, "p_float", "VtArray<float>", "vertex", 4)
        self.check_primvar(prim, "p_color", "VtArray<GfVec4f>", "vertex", 4)
        self.check_primvar(prim, "p_byte_color", "VtArray<GfVec4f>", "vertex", 4)
        self.check_primvar(prim, "p_vec2", "VtArray<GfVec2f>", "vertex", 4)
        self.check_primvar(prim, "p_vec3", "VtArray<GfVec3f>", "vertex", 4)
        self.check_primvar(prim, "p_quat", "VtArray<GfQuatf>", "vertex", 4)
        self.check_primvar_missing(prim, "p_mat4x4")

        self.check_primvar_missing(prim, "e_bool")
        self.check_primvar_missing(prim, "e_int8")
        self.check_primvar_missing(prim, "e_int32")
        self.check_primvar_missing(prim, "e_float")
        self.check_primvar_missing(prim, "e_color")
        self.check_primvar_missing(prim, "e_byte_color")
        self.check_primvar_missing(prim, "e_vec2")
        self.check_primvar_missing(prim, "e_vec3")
        self.check_primvar_missing(prim, "e_quat")
        self.check_primvar_missing(prim, "e_mat4x4")

        self.check_primvar(prim, "f_bool", "VtArray<bool>", "uniform", 1)
        self.check_primvar(prim, "f_int8", "VtArray<unsigned char>", "uniform", 1)
        self.check_primvar(prim, "f_int32", "VtArray<int>", "uniform", 1)
        self.check_primvar(prim, "f_float", "VtArray<float>", "uniform", 1)
        self.check_primvar(prim, "f_color", "VtArray<GfVec4f>", "uniform", 1)
        self.check_primvar(prim, "f_byte_color", "VtArray<GfVec4f>", "uniform", 1)
        self.check_primvar(prim, "displayColor", "VtArray<GfVec3f>", "uniform", 1)
        self.check_primvar(prim, "f_vec2", "VtArray<GfVec2f>", "uniform", 1)
        self.check_primvar(prim, "f_vec3", "VtArray<GfVec3f>", "uniform", 1)
        self.check_primvar(prim, "f_quat", "VtArray<GfQuatf>", "uniform", 1)
        self.check_primvar_missing(prim, "f_mat4x4")

        self.check_primvar(prim, "fc_bool", "VtArray<bool>", "faceVarying", 4)
        self.check_primvar(prim, "fc_int8", "VtArray<unsigned char>", "faceVarying", 4)
        self.check_primvar(prim, "fc_int32", "VtArray<int>", "faceVarying", 4)
        self.check_primvar(prim, "fc_float", "VtArray<float>", "faceVarying", 4)
        self.check_primvar(prim, "fc_color", "VtArray<GfVec4f>", "faceVarying", 4)
        self.check_primvar(prim, "fc_byte_color", "VtArray<GfVec4f>", "faceVarying", 4)
        self.check_primvar(prim, "fc_vec2", "VtArray<GfVec2f>", "faceVarying", 4)
        self.check_primvar(prim, "fc_vec3", "VtArray<GfVec3f>", "faceVarying", 4)
        self.check_primvar(prim, "fc_quat", "VtArray<GfQuatf>", "faceVarying", 4)
        self.check_primvar_missing(prim, "fc_mat4x4")

        prim = stage.GetPrimAtPath("/root/Curve_base/Curves/Curves")

        self.check_primvar(prim, "p_bool", "VtArray<bool>", "vertex", 24)
        self.check_primvar(prim, "p_int8", "VtArray<unsigned char>", "vertex", 24)
        self.check_primvar(prim, "p_int32", "VtArray<int>", "vertex", 24)
        self.check_primvar(prim, "p_float", "VtArray<float>", "vertex", 24)
        self.check_primvar(prim, "p_color", "VtArray<GfVec4f>", "vertex", 24)
        self.check_primvar(prim, "p_byte_color", "VtArray<GfVec4f>", "vertex", 24)
        self.check_primvar(prim, "p_vec2", "VtArray<GfVec2f>", "vertex", 24)
        self.check_primvar(prim, "p_vec3", "VtArray<GfVec3f>", "vertex", 24)
        self.check_primvar(prim, "p_quat", "VtArray<GfQuatf>", "vertex", 24)
        self.check_primvar_missing(prim, "p_mat4x4")

        self.check_primvar(prim, "sp_bool", "VtArray<bool>", "uniform", 2)
        self.check_primvar(prim, "sp_int8", "VtArray<unsigned char>", "uniform", 2)
        self.check_primvar(prim, "sp_int32", "VtArray<int>", "uniform", 2)
        self.check_primvar(prim, "sp_float", "VtArray<float>", "uniform", 2)
        self.check_primvar(prim, "sp_color", "VtArray<GfVec4f>", "uniform", 2)
        self.check_primvar(prim, "sp_byte_color", "VtArray<GfVec4f>", "uniform", 2)
        self.check_primvar(prim, "sp_vec2", "VtArray<GfVec2f>", "uniform", 2)
        self.check_primvar(prim, "sp_vec3", "VtArray<GfVec3f>", "uniform", 2)
        self.check_primvar(prim, "sp_quat", "VtArray<GfQuatf>", "uniform", 2)
        self.check_primvar_missing(prim, "sp_mat4x4")

        prim = stage.GetPrimAtPath("/root/Curve_bezier_base/Curves_bezier/Curves")

        self.check_primvar(prim, "p_bool", "VtArray<bool>", "varying", 10)
        self.check_primvar(prim, "p_int8", "VtArray<unsigned char>", "varying", 10)
        self.check_primvar(prim, "p_int32", "VtArray<int>", "varying", 10)
        self.check_primvar(prim, "p_float", "VtArray<float>", "varying", 10)
        self.check_primvar(prim, "p_color", "VtArray<GfVec4f>", "varying", 10)
        self.check_primvar(prim, "p_byte_color", "VtArray<GfVec4f>", "varying", 10)
        self.check_primvar(prim, "p_vec2", "VtArray<GfVec2f>", "varying", 10)
        self.check_primvar(prim, "p_vec3", "VtArray<GfVec3f>", "varying", 10)
        self.check_primvar(prim, "p_quat", "VtArray<GfQuatf>", "varying", 10)
        self.check_primvar_missing(prim, "p_mat4x4")

        self.check_primvar(prim, "sp_bool", "VtArray<bool>", "uniform", 3)
        self.check_primvar(prim, "sp_int8", "VtArray<unsigned char>", "uniform", 3)
        self.check_primvar(prim, "sp_int32", "VtArray<int>", "uniform", 3)
        self.check_primvar(prim, "sp_float", "VtArray<float>", "uniform", 3)
        self.check_primvar(prim, "sp_color", "VtArray<GfVec4f>", "uniform", 3)
        self.check_primvar(prim, "sp_byte_color", "VtArray<GfVec4f>", "uniform", 3)
        self.check_primvar(prim, "sp_vec2", "VtArray<GfVec2f>", "uniform", 3)
        self.check_primvar(prim, "sp_vec3", "VtArray<GfVec3f>", "uniform", 3)
        self.check_primvar(prim, "sp_quat", "VtArray<GfQuatf>", "uniform", 3)
        self.check_primvar_missing(prim, "sp_mat4x4")

    def test_export_attributes_varying(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_attribute_varying_test.blend"))
        # Ensure the simulation zone data is baked for all relevant frames...
        for frame in range(1, 16):
            bpy.context.scene.frame_set(frame)
        bpy.context.scene.frame_set(1)

        export_path = self.tempdir / "usd_attribute_varying_test.usda"
        self.export_and_validate(filepath=str(export_path), export_animation=True, evaluation_mode="RENDER")

        stage = Usd.Stage.Open(str(export_path))
        sparse_frames = [4.0, 5.0, 8.0, 9.0, 12.0, 13.0]

        #
        # Validate Mesh data
        #
        mesh1 = UsdGeom.Mesh(stage.GetPrimAtPath("/root/mesh1/mesh1"))
        mesh2 = UsdGeom.Mesh(stage.GetPrimAtPath("/root/mesh2/mesh2"))
        mesh3 = UsdGeom.Mesh(stage.GetPrimAtPath("/root/mesh3/mesh3"))

        # Positions (should be sparsely written)
        self.assertEqual(mesh1.GetPointsAttr().GetTimeSamples(), sparse_frames)
        self.assertEqual(mesh2.GetPointsAttr().GetTimeSamples(), [])
        self.assertEqual(mesh3.GetPointsAttr().GetTimeSamples(), [])
        # Velocity (should be sparsely written)
        self.assertEqual(mesh1.GetVelocitiesAttr().GetTimeSamples(), [])
        self.assertEqual(mesh2.GetVelocitiesAttr().GetTimeSamples(), sparse_frames)
        self.assertEqual(mesh3.GetVelocitiesAttr().GetTimeSamples(), [])
        # Regular primvar (should be sparsely written)
        self.assertEqual(UsdGeom.PrimvarsAPI(mesh1).GetPrimvar("test").GetTimeSamples(), [])
        self.assertEqual(UsdGeom.PrimvarsAPI(mesh2).GetPrimvar("test").GetTimeSamples(), [])
        self.assertEqual(UsdGeom.PrimvarsAPI(mesh3).GetPrimvar("test").GetTimeSamples(), sparse_frames)
        # Extents of the mesh (should be sparsely written)
        self.assertEqual(UsdGeom.Boundable(mesh1).GetExtentAttr().GetTimeSamples(), sparse_frames)
        self.assertEqual(UsdGeom.Boundable(mesh2).GetExtentAttr().GetTimeSamples(), [])
        self.assertEqual(UsdGeom.Boundable(mesh3).GetExtentAttr().GetTimeSamples(), [])

        #
        # Validate PointCloud data
        #
        points1 = UsdGeom.Points(stage.GetPrimAtPath("/root/pointcloud1/PointCloud"))
        points2 = UsdGeom.Points(stage.GetPrimAtPath("/root/pointcloud2/PointCloud"))
        points3 = UsdGeom.Points(stage.GetPrimAtPath("/root/pointcloud3/PointCloud"))
        points4 = UsdGeom.Points(stage.GetPrimAtPath("/root/pointcloud4/PointCloud"))

        # Positions (should be sparsely written)
        self.assertEqual(points1.GetPointsAttr().GetTimeSamples(), sparse_frames)
        self.assertEqual(points2.GetPointsAttr().GetTimeSamples(), [])
        self.assertEqual(points3.GetPointsAttr().GetTimeSamples(), [])
        self.assertEqual(points4.GetPointsAttr().GetTimeSamples(), [])
        # Velocity (should be sparsely written)
        self.assertEqual(points1.GetVelocitiesAttr().GetTimeSamples(), [])
        self.assertEqual(points2.GetVelocitiesAttr().GetTimeSamples(), sparse_frames)
        self.assertEqual(points3.GetVelocitiesAttr().GetTimeSamples(), [])
        self.assertEqual(points4.GetVelocitiesAttr().GetTimeSamples(), [])
        # Radius (should be sparsely written)
        self.assertEqual(points1.GetWidthsAttr().GetTimeSamples(), [])
        self.assertEqual(points2.GetWidthsAttr().GetTimeSamples(), [])
        self.assertEqual(points3.GetWidthsAttr().GetTimeSamples(), sparse_frames)
        self.assertEqual(points4.GetWidthsAttr().GetTimeSamples(), [])
        # Regular primvar (should be sparsely written)
        self.assertEqual(UsdGeom.PrimvarsAPI(points1).GetPrimvar("test").GetTimeSamples(), [])
        self.assertEqual(UsdGeom.PrimvarsAPI(points2).GetPrimvar("test").GetTimeSamples(), [])
        self.assertEqual(UsdGeom.PrimvarsAPI(points3).GetPrimvar("test").GetTimeSamples(), [])
        self.assertEqual(UsdGeom.PrimvarsAPI(points4).GetPrimvar("test").GetTimeSamples(), sparse_frames)
        # Extents of the point cloud (should be sparsely written)
        self.assertEqual(UsdGeom.Boundable(points1).GetExtentAttr().GetTimeSamples(), sparse_frames)
        self.assertEqual(UsdGeom.Boundable(points2).GetExtentAttr().GetTimeSamples(), [])
        self.assertEqual(UsdGeom.Boundable(points3).GetExtentAttr().GetTimeSamples(), sparse_frames)
        self.assertEqual(UsdGeom.Boundable(points4).GetExtentAttr().GetTimeSamples(), [])

        #
        # Validate BasisCurve data
        #
        curves1 = UsdGeom.BasisCurves(stage.GetPrimAtPath("/root/curves_plane1/curves1/Curves"))
        curves2 = UsdGeom.BasisCurves(stage.GetPrimAtPath("/root/curves_plane2/curves2/Curves"))
        curves3 = UsdGeom.BasisCurves(stage.GetPrimAtPath("/root/curves_plane3/curves3/Curves"))
        curves4 = UsdGeom.BasisCurves(stage.GetPrimAtPath("/root/curves_plane4/curves4/Curves"))

        # Positions (should be sparsely written)
        self.assertEqual(curves1.GetPointsAttr().GetTimeSamples(), sparse_frames)
        self.assertEqual(curves2.GetPointsAttr().GetTimeSamples(), [])
        self.assertEqual(curves3.GetPointsAttr().GetTimeSamples(), [])
        self.assertEqual(curves4.GetPointsAttr().GetTimeSamples(), [])
        # Velocity (should be sparsely written)
        self.assertEqual(curves1.GetVelocitiesAttr().GetTimeSamples(), [])
        self.assertEqual(curves2.GetVelocitiesAttr().GetTimeSamples(), sparse_frames)
        self.assertEqual(curves3.GetVelocitiesAttr().GetTimeSamples(), [])
        self.assertEqual(curves4.GetVelocitiesAttr().GetTimeSamples(), [])
        # Radius (should be sparsely written)
        self.assertEqual(curves1.GetWidthsAttr().GetTimeSamples(), [])
        self.assertEqual(curves2.GetWidthsAttr().GetTimeSamples(), [])
        self.assertEqual(curves3.GetWidthsAttr().GetTimeSamples(), sparse_frames)
        self.assertEqual(curves4.GetWidthsAttr().GetTimeSamples(), [])
        # Regular primvar (should be sparsely written)
        self.assertEqual(UsdGeom.PrimvarsAPI(curves1).GetPrimvar("test").GetTimeSamples(), [])
        self.assertEqual(UsdGeom.PrimvarsAPI(curves2).GetPrimvar("test").GetTimeSamples(), [])
        self.assertEqual(UsdGeom.PrimvarsAPI(curves3).GetPrimvar("test").GetTimeSamples(), [])
        self.assertEqual(UsdGeom.PrimvarsAPI(curves4).GetPrimvar("test").GetTimeSamples(), sparse_frames)

    def test_export_mesh_subd(self):
        """Test exporting Subdivision Surface attributes and values"""
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_mesh_subd.blend"))
        export_path = self.tempdir / "usd_mesh_subd.usda"
        self.export_and_validate(
            filepath=str(export_path),
            export_subdivision='BEST_MATCH',
            evaluation_mode="RENDER",
        )

        stage = Usd.Stage.Open(str(export_path))

        mesh = UsdGeom.Mesh(stage.GetPrimAtPath("/root/uv_smooth_none_boundary_smooth_all/mesh1"))
        self.assertEqual(mesh.GetSubdivisionSchemeAttr().Get(), 'catmullClark')
        self.assertEqual(mesh.GetFaceVaryingLinearInterpolationAttr().Get(), 'all')
        self.assertEqual(mesh.GetInterpolateBoundaryAttr().Get(), 'edgeOnly')

        mesh = UsdGeom.Mesh(stage.GetPrimAtPath("/root/uv_smooth_corners_boundary_smooth_all/mesh2"))
        self.assertEqual(mesh.GetSubdivisionSchemeAttr().Get(), 'catmullClark')
        self.assertEqual(mesh.GetFaceVaryingLinearInterpolationAttr().Get(), 'cornersOnly')
        self.assertEqual(mesh.GetInterpolateBoundaryAttr().Get(), 'edgeOnly')

        mesh = UsdGeom.Mesh(stage.GetPrimAtPath("/root/uv_smooth_corners_junctions_boundary_smooth_all/mesh3"))
        self.assertEqual(mesh.GetSubdivisionSchemeAttr().Get(), 'catmullClark')
        self.assertEqual(mesh.GetFaceVaryingLinearInterpolationAttr().Get(), 'cornersPlus1')
        self.assertEqual(mesh.GetInterpolateBoundaryAttr().Get(), 'edgeOnly')

        mesh = UsdGeom.Mesh(stage.GetPrimAtPath("/root/uv_smooth_corners_junctions_concave_boundary_smooth_all/mesh4"))
        self.assertEqual(mesh.GetSubdivisionSchemeAttr().Get(), 'catmullClark')
        self.assertEqual(mesh.GetFaceVaryingLinearInterpolationAttr().Get(), 'cornersPlus2')
        self.assertEqual(mesh.GetInterpolateBoundaryAttr().Get(), 'edgeOnly')

        mesh = UsdGeom.Mesh(stage.GetPrimAtPath("/root/uv_smooth_boundaries_boundary_smooth_all/mesh5"))
        self.assertEqual(mesh.GetSubdivisionSchemeAttr().Get(), 'catmullClark')
        self.assertEqual(mesh.GetFaceVaryingLinearInterpolationAttr().Get(), 'boundaries')
        self.assertEqual(mesh.GetInterpolateBoundaryAttr().Get(), 'edgeOnly')

        mesh = UsdGeom.Mesh(stage.GetPrimAtPath("/root/uv_smooth_all_boundary_smooth_all/mesh6"))
        self.assertEqual(mesh.GetSubdivisionSchemeAttr().Get(), 'catmullClark')
        self.assertEqual(mesh.GetFaceVaryingLinearInterpolationAttr().Get(), 'none')
        self.assertEqual(mesh.GetInterpolateBoundaryAttr().Get(), 'edgeOnly')

        mesh = UsdGeom.Mesh(stage.GetPrimAtPath("/root/uv_smooth_boundaries_boundary_smooth_keep/mesh7"))
        self.assertEqual(mesh.GetSubdivisionSchemeAttr().Get(), 'catmullClark')
        self.assertEqual(mesh.GetFaceVaryingLinearInterpolationAttr().Get(), 'boundaries')
        self.assertEqual(mesh.GetInterpolateBoundaryAttr().Get(), 'edgeAndCorner')

        mesh = UsdGeom.Mesh(stage.GetPrimAtPath("/root/crease_verts/crease_verts"))
        self.assertEqual(mesh.GetSubdivisionSchemeAttr().Get(), 'catmullClark')
        self.assertEqual(mesh.GetFaceVaryingLinearInterpolationAttr().Get(), 'boundaries')
        self.assertEqual(mesh.GetInterpolateBoundaryAttr().Get(), 'edgeOnly')
        self.assertEqual(len(mesh.GetCornerIndicesAttr().Get()), 7)
        usd_vert_sharpness = mesh.GetCornerSharpnessesAttr().Get()
        self.assertEqual(len(usd_vert_sharpness), 7)
        # A 1.0 crease is INFINITE (10) in USD
        self.assertAlmostEqual(min(usd_vert_sharpness), 0.1, 5)
        self.assertEqual(len([sharp for sharp in usd_vert_sharpness if sharp < 10]), 6)
        self.assertEqual(len([sharp for sharp in usd_vert_sharpness if sharp == 10]), 1)

        mesh = UsdGeom.Mesh(stage.GetPrimAtPath("/root/crease_edge/crease_edge"))
        self.assertEqual(mesh.GetSubdivisionSchemeAttr().Get(), 'catmullClark')
        self.assertEqual(mesh.GetFaceVaryingLinearInterpolationAttr().Get(), 'boundaries')
        self.assertEqual(mesh.GetInterpolateBoundaryAttr().Get(), 'edgeOnly')
        self.assertEqual(len(mesh.GetCreaseIndicesAttr().Get()), 20)
        usd_crease_lengths = mesh.GetCreaseLengthsAttr().Get()
        self.assertEqual(len(usd_crease_lengths), 10)
        self.assertTrue(all([length == 2 for length in usd_crease_lengths]))
        usd_crease_sharpness = mesh.GetCreaseSharpnessesAttr().Get()
        self.assertEqual(len(usd_crease_sharpness), 10)
        # A 1.0 crease is INFINITE (10) in USD
        self.assertAlmostEqual(min(usd_crease_sharpness), 0.1, 5)
        self.assertEqual(len([sharp for sharp in usd_crease_sharpness if sharp < 10]), 9)
        self.assertEqual(len([sharp for sharp in usd_crease_sharpness if sharp == 10]), 1)

    def test_export_mesh_triangulate(self):
        """Test exporting with different triangulation options for meshes."""

        # Use the current scene to create simple geometry to triangulate
        bpy.ops.mesh.primitive_plane_add(size=1)
        bpy.ops.mesh.primitive_circle_add(fill_type='NGON', radius=1, vertices=7)

        # We assume that triangulation is thoroughly tested elsewhere. Here we are only interested
        # in checking that USD passes its operator properties through correctly. We use a minimal
        # combination of quad and ngon methods to test.
        tri_export_path1 = self.tempdir / "usd_mesh_tri_setup1.usda"
        self.export_and_validate(
            filepath=str(tri_export_path1),
            triangulate_meshes=True,
            quad_method='FIXED',
            ngon_method='BEAUTY',
            evaluation_mode="RENDER",
        )

        tri_export_path2 = self.tempdir / "usd_mesh_tri_setup2.usda"
        self.export_and_validate(
            filepath=str(tri_export_path2),
            triangulate_meshes=True,
            quad_method='FIXED_ALTERNATE',
            ngon_method='CLIP',
            evaluation_mode="RENDER",
        )

        stage1 = Usd.Stage.Open(str(tri_export_path1))
        stage2 = Usd.Stage.Open(str(tri_export_path2))

        # The Plane should have different vertex ordering because of the quad methods chosen
        plane1 = UsdGeom.Mesh(stage1.GetPrimAtPath("/root/Plane/Plane"))
        plane2 = UsdGeom.Mesh(stage2.GetPrimAtPath("/root/Plane/Plane"))
        indices1 = plane1.GetFaceVertexIndicesAttr().Get()
        indices2 = plane2.GetFaceVertexIndicesAttr().Get()
        self.assertEqual(len(indices1), 6)
        self.assertEqual(len(indices2), 6)
        self.assertNotEqual(indices1, indices2)

        # The Circle should have different vertex ordering because of the ngon methods chosen
        circle1 = UsdGeom.Mesh(stage1.GetPrimAtPath("/root/Circle/Circle"))
        circle2 = UsdGeom.Mesh(stage2.GetPrimAtPath("/root/Circle/Circle"))
        indices1 = circle1.GetFaceVertexIndicesAttr().Get()
        indices2 = circle2.GetFaceVertexIndicesAttr().Get()
        self.assertEqual(len(indices1), 15)
        self.assertEqual(len(indices2), 15)
        self.assertNotEqual(indices1, indices2)

    def test_export_curves(self):
        """Test exporting Curve types"""
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_curves_test.blend"))
        export_path = self.tempdir / "usd_curves_test.usda"
        self.export_and_validate(filepath=str(export_path), evaluation_mode="RENDER")

        stage = Usd.Stage.Open(str(export_path))

        def check_basis_curve(prim, basis, curve_type, wrap, vert_counts, extent):
            self.assertEqual(prim.GetBasisAttr().Get(), basis)
            self.assertEqual(prim.GetTypeAttr().Get(), curve_type)
            self.assertEqual(prim.GetWrapAttr().Get(), wrap)
            self.assertEqual(prim.GetWidthsInterpolation(), "varying" if basis == "bezier" else "vertex")
            self.assertEqual(prim.GetCurveVertexCountsAttr().Get(), vert_counts)
            usd_extent = prim.GetExtentAttr().Get()
            self.assertEqual(self.round_vector(usd_extent[0]), extent[0])
            self.assertEqual(self.round_vector(usd_extent[1]), extent[1])

        def check_nurbs_curve(prim, cyclic, orders, vert_counts, knots_count, extent):
            self.assertEqual(prim.GetOrderAttr().Get(), orders)
            self.assertEqual(prim.GetCurveVertexCountsAttr().Get(), vert_counts)
            self.assertEqual(prim.GetWidthsInterpolation(), "vertex")
            knots = prim.GetKnotsAttr().Get()
            usd_extent = prim.GetExtentAttr().Get()
            self.assertEqual(self.round_vector(usd_extent[0]), extent[0])
            self.assertEqual(self.round_vector(usd_extent[1]), extent[1])

            curve_count = len(vert_counts)
            self.assertEqual(len(knots), knots_count * curve_count)
            if not cyclic:
                for i in range(0, curve_count):
                    zeroth_knot = i * len(knots) // curve_count
                    self.assertEqual(knots[zeroth_knot], knots[zeroth_knot + 1], "Knots start rule violated")
                    self.assertEqual(
                        knots[zeroth_knot + knots_count - 1],
                        knots[zeroth_knot + knots_count - 2],
                        "Knots end rule violated")
            else:
                self.assertEqual(curve_count, 1, "Validation is only correct for 1 cyclic curve currently")
                self.assertEqual(
                    knots[0], knots[1] - (knots[knots_count - 2] - knots[knots_count - 3]), "Knots rule violated")
                self.assertEqual(
                    knots[knots_count - 1], knots[knots_count - 2] + (knots[2] - knots[1]), "Knots rule violated")

        # Contains 3 CatmullRom curves
        curve = UsdGeom.BasisCurves(stage.GetPrimAtPath("/root/Cube/Curves/Curves"))
        check_basis_curve(
            curve, "catmullRom", "cubic", "pinned", [8, 8, 8], [[-0.3784, -0.0866, 1], [0.2714, -0.0488, 1.3]])

        # Contains 1 Bezier curve
        curve = UsdGeom.BasisCurves(stage.GetPrimAtPath("/root/BezierCurve/BezierCurve"))
        check_basis_curve(curve, "bezier", "cubic", "nonperiodic", [7], [[-2.644, -0.0777, 0], [1, 0.9815, 0]])

        # Contains 1 Bezier curve
        curve = UsdGeom.BasisCurves(stage.GetPrimAtPath("/root/BezierCircle/BezierCircle"))
        check_basis_curve(curve, "bezier", "cubic", "periodic", [12], [[-1, -1, 0], [1, 1, 0]])

        # Contains 2 NURBS curves
        curve = UsdGeom.NurbsCurves(stage.GetPrimAtPath("/root/NurbsCurve/NurbsCurve"))
        check_nurbs_curve(curve, False, [4, 4], [6, 6], 10, [[-0.75, -1.6898, -0.0117], [2.0896, 0.9583, 0.0293]])

        # Contains 1 NURBS curve
        curve = UsdGeom.NurbsCurves(stage.GetPrimAtPath("/root/NurbsCircle/NurbsCircle"))
        check_nurbs_curve(curve, True, [3], [8], 13, [[-1, -1, 0], [1, 1, 0]])

    def test_export_animation(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_anim_test.blend"))
        export_path = self.tempdir / "usd_anim_test.usda"
        self.export_and_validate(
            filepath=str(export_path),
            export_animation=True,
            evaluation_mode="RENDER",
        )

        stage = Usd.Stage.Open(str(export_path))

        # Validate the simple object animation
        prim = stage.GetPrimAtPath("/root/cube_anim_xform")
        self.assertEqual(prim.GetTypeName(), "Xform")
        loc_samples = UsdGeom.Xformable(prim).GetTranslateOp().GetTimeSamples()
        rot_samples = UsdGeom.Xformable(prim).GetRotateXYZOp().GetTimeSamples()
        scale_samples = UsdGeom.Xformable(prim).GetScaleOp().GetTimeSamples()
        self.assertEqual(loc_samples, [1.0, 2.0, 3.0, 4.0])
        self.assertEqual(rot_samples, [1.0])
        self.assertEqual(scale_samples, [1.0])

        prim = stage.GetPrimAtPath("/root/cube_anim_xform/cube_anim_child")
        self.assertEqual(prim.GetTypeName(), "Xform")
        loc_samples = UsdGeom.Xformable(prim).GetTranslateOp().GetTimeSamples()
        rot_samples = UsdGeom.Xformable(prim).GetRotateXYZOp().GetTimeSamples()
        scale_samples = UsdGeom.Xformable(prim).GetScaleOp().GetTimeSamples()
        self.assertEqual(loc_samples, [1.0])
        self.assertEqual(rot_samples, [1.0, 2.0, 3.0, 4.0])
        self.assertEqual(scale_samples, [1.0])

        # Validate the armature animation
        prim = stage.GetPrimAtPath("/root/Armature/Armature")
        self.assertEqual(prim.GetTypeName(), "Skeleton")
        prim_skel = UsdSkel.BindingAPI(prim)
        anim = UsdSkel.Animation(prim_skel.GetAnimationSource())
        self.assertEqual(anim.GetPrim().GetName(), "ArmatureAction_001")
        self.assertEqual(anim.GetJointsAttr().Get(),
                         ['Bone',
                          'Bone/Bone_001',
                          'Bone/Bone_001/Bone_002',
                          'Bone/Bone_001/Bone_002/Bone_003',
                          'Bone/Bone_001/Bone_002/Bone_003/Bone_004'])
        loc_samples = anim.GetTranslationsAttr().GetTimeSamples()
        rot_samples = anim.GetRotationsAttr().GetTimeSamples()
        scale_samples = anim.GetScalesAttr().GetTimeSamples()
        self.assertEqual(loc_samples, [])
        self.assertEqual(rot_samples, [1.0, 2.0, 3.0])
        self.assertEqual(scale_samples, [])

        # Validate the shape key animation
        prim = stage.GetPrimAtPath("/root/cube_anim_keys")
        self.assertEqual(prim.GetTypeName(), "SkelRoot")
        prim_skel = UsdSkel.BindingAPI(prim.GetPrimAtPath("cube_anim_keys"))
        self.assertEqual(prim_skel.GetBlendShapesAttr().Get(), ['Key_1'])
        prim_skel = UsdSkel.BindingAPI(prim.GetPrimAtPath("Skel"))
        anim = UsdSkel.Animation(prim_skel.GetAnimationSource())
        weight_samples = anim.GetBlendShapeWeightsAttr().GetTimeSamples()
        self.assertEqual(weight_samples, [1.0, 2.0, 3.0, 4.0, 5.0])

    def test_export_volumes(self):
        """Test various combinations of volume export including with all supported volume modifiers."""

        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_volumes.blend"))
        # Ensure the simulation zone data is baked for all relevant frames...
        for frame in range(4, 15):
            bpy.context.scene.frame_set(frame)
        bpy.context.scene.frame_set(4)

        export_path = self.tempdir / "usd_volumes.usda"
        self.export_and_validate(filepath=str(export_path), export_animation=True, evaluation_mode="RENDER")

        stage = Usd.Stage.Open(str(export_path))

        # Validate that we see some form of time varyability across the Volume prim's extents and
        # file paths. The data should be sparse so it should only be written on the frames which
        # change.

        # File sequence
        vol_fileseq = UsdVol.Volume(stage.GetPrimAtPath("/root/vol_filesequence/vol_filesequence"))
        density = UsdVol.OpenVDBAsset(stage.GetPrimAtPath("/root/vol_filesequence/vol_filesequence/density_noise"))
        flame = UsdVol.OpenVDBAsset(stage.GetPrimAtPath("/root/vol_filesequence/vol_filesequence/flame_noise"))
        self.assertEqual(vol_fileseq.GetExtentAttr().GetTimeSamples(), [10.0, 11.0])
        self.assertEqual(density.GetFieldNameAttr().GetTimeSamples(), [])
        self.assertEqual(density.GetFilePathAttr().GetTimeSamples(), [8.0, 9.0, 10.0, 11.0, 12.0, 13.0])
        self.assertEqual(flame.GetFieldNameAttr().GetTimeSamples(), [])
        self.assertEqual(flame.GetFilePathAttr().GetTimeSamples(), [8.0, 9.0, 10.0, 11.0, 12.0, 13.0])

        # Mesh To Volume
        vol_mesh2vol = UsdVol.Volume(stage.GetPrimAtPath("/root/vol_mesh2vol/vol_mesh2vol"))
        density = UsdVol.OpenVDBAsset(stage.GetPrimAtPath("/root/vol_mesh2vol/vol_mesh2vol/density"))
        self.assertEqual(vol_mesh2vol.GetExtentAttr().GetTimeSamples(),
                         [6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0])
        self.assertEqual(density.GetFieldNameAttr().GetTimeSamples(), [])
        self.assertEqual(density.GetFilePathAttr().GetTimeSamples(),
                         [4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0])

        # Volume Displace
        vol_displace = UsdVol.Volume(stage.GetPrimAtPath("/root/vol_displace/vol_displace"))
        unnamed = UsdVol.OpenVDBAsset(stage.GetPrimAtPath("/root/vol_displace/vol_displace/_"))
        self.assertEqual(vol_displace.GetExtentAttr().GetTimeSamples(),
                         [5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0])
        self.assertEqual(unnamed.GetFieldNameAttr().GetTimeSamples(), [])
        self.assertEqual(unnamed.GetFilePathAttr().GetTimeSamples(),
                         [4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0])

        # Geometry Node simulation
        vol_sim = UsdVol.Volume(stage.GetPrimAtPath("/root/vol_sim/Volume"))
        density = UsdVol.OpenVDBAsset(stage.GetPrimAtPath("/root/vol_sim/Volume/density"))
        self.assertEqual(vol_sim.GetExtentAttr().GetTimeSamples(),
                         [4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0])
        self.assertEqual(density.GetFieldNameAttr().GetTimeSamples(), [])
        self.assertEqual(density.GetFilePathAttr().GetTimeSamples(),
                         [4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0])

    def test_export_xform_ops(self):
        """Test exporting different xform operation modes."""

        # Create a simple scene and export using each of our xform op modes
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        loc = [1, 2, 3]
        rot = [math.pi / 4, 0, math.pi / 8]
        scale = [1, 2, 3]

        bpy.ops.mesh.primitive_plane_add(location=loc, rotation=rot)
        bpy.data.objects[0].scale = scale

        test_path1 = self.tempdir / "temp_xform_trs_test.usda"
        self.export_and_validate(filepath=str(test_path1), xform_op_mode='TRS')

        test_path2 = self.tempdir / "temp_xform_tos_test.usda"
        self.export_and_validate(filepath=str(test_path2), xform_op_mode='TOS')

        test_path3 = self.tempdir / "temp_xform_mat_test.usda"
        self.export_and_validate(filepath=str(test_path3), xform_op_mode='MAT')

        # Validate relevant details for each case
        stage = Usd.Stage.Open(str(test_path1))
        xf = UsdGeom.Xformable(stage.GetPrimAtPath("/root/Plane"))
        rot_degs = [math.degrees(rot[0]), math.degrees(rot[1]), math.degrees(rot[2])]
        self.assertEqual(xf.GetXformOpOrderAttr().Get(), ['xformOp:translate', 'xformOp:rotateXYZ', 'xformOp:scale'])
        self.assertEqual(self.round_vector(xf.GetTranslateOp().Get()), loc)
        self.assertEqual(self.round_vector(xf.GetRotateXYZOp().Get()), rot_degs)
        self.assertEqual(self.round_vector(xf.GetScaleOp().Get()), scale)

        stage = Usd.Stage.Open(str(test_path2))
        xf = UsdGeom.Xformable(stage.GetPrimAtPath("/root/Plane"))
        orient_quat = xf.GetOrientOp().Get()
        self.assertEqual(xf.GetXformOpOrderAttr().Get(), ['xformOp:translate', 'xformOp:orient', 'xformOp:scale'])
        self.assertEqual(self.round_vector(xf.GetTranslateOp().Get()), loc)
        self.assertEqual(round(orient_quat.GetReal(), 4), 0.9061)
        self.assertEqual(self.round_vector(orient_quat.GetImaginary()), [0.3753, 0.0747, 0.1802])
        self.assertEqual(self.round_vector(xf.GetScaleOp().Get()), scale)

        stage = Usd.Stage.Open(str(test_path3))
        xf = UsdGeom.Xformable(stage.GetPrimAtPath("/root/Plane"))
        mat = xf.GetTransformOp().Get()
        mat = [
            self.round_vector(mat[0]), self.round_vector(mat[1]), self.round_vector(mat[2]), self.round_vector(mat[3])
        ]
        expected = [
            [0.9239, 0.3827, 0.0, 0.0],
            [-0.5412, 1.3066, 1.4142, 0.0],
            [0.8118, -1.9598, 2.1213, 0.0],
            [1.0, 2.0, 3.0, 1.0]
        ]
        self.assertEqual(xf.GetXformOpOrderAttr().Get(), ['xformOp:transform'])
        self.assertEqual(mat, expected)

    def test_export_orientation(self):
        """Test exporting different orientation configurations."""

        # Using the empty scene is fine for this
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))

        test_path1 = self.tempdir / "temp_orientation_yup.usda"
        self.export_and_validate(
            filepath=str(test_path1),
            convert_orientation=True,
            export_global_forward_selection='NEGATIVE_Z',
            export_global_up_selection='Y')

        test_path2 = self.tempdir / "temp_orientation_zup_rev.usda"
        self.export_and_validate(
            filepath=str(test_path2),
            convert_orientation=True,
            export_global_forward_selection='NEGATIVE_Y',
            export_global_up_selection='Z')

        stage = Usd.Stage.Open(str(test_path1))
        xf = UsdGeom.Xformable(stage.GetPrimAtPath("/root"))
        self.assertEqual(self.round_vector(xf.GetRotateXYZOp().Get()), [-90, 0, 0])

        stage = Usd.Stage.Open(str(test_path2))
        xf = UsdGeom.Xformable(stage.GetPrimAtPath("/root"))
        self.assertEqual(self.round_vector(xf.GetRotateXYZOp().Get()), [0, 0, 180])

    def test_materialx_network(self):
        """Test exporting that a MaterialX export makes it out alright"""
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_export.blend"))
        export_path = self.tempdir / "materialx.usda"

        # USD currently has an issue where embedded MaterialX graphs cause validation to fail.
        # Skip validation and just run a regular export until this is fixed.
        # See: https://github.com/PixarAnimationStudios/OpenUSD/pull/3243
        res = bpy.ops.wm.usd_export(
            filepath=str(export_path),
            export_materials=True,
            generate_materialx_network=True,
            evaluation_mode="RENDER",
        )
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {export_path}")

        stage = Usd.Stage.Open(str(export_path))
        material_prim = stage.GetPrimAtPath("/root/_materials/Material")
        self.assertTrue(material_prim, "Could not find Material prim")

        material = UsdShade.Material(material_prim)
        mtlx_output = material.GetOutput("mtlx:surface")
        self.assertTrue(mtlx_output, "Could not find mtlx output")

        connection, source_name, _ = UsdShade.ConnectableAPI.GetConnectedSource(
            mtlx_output
        ) or [None, None, None]

        self.assertTrue((connection and source_name), "Could not find mtlx output source")

        shader = UsdShade.Shader(connection.GetPrim())
        self.assertTrue(shader, "Connected prim is not a shader")

        shader_id = shader.GetIdAttr().Get()
        self.assertEqual(shader_id, "ND_standard_surface_surfaceshader", "Shader is not a Standard Surface")

    def test_hooks(self):
        """Validate USD Hook integration for both import and export"""

        # Create a simple scene with 1 object and 1 material
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))
        material = bpy.data.materials.new(name="test_material")
        material.use_nodes = True
        bpy.ops.mesh.primitive_plane_add()
        bpy.data.objects[0].data.materials.append(material)

        # Register both USD hooks
        bpy.utils.register_class(USDHook1)
        bpy.utils.register_class(USDHook2)

        # Instruct them to do various actions inside their implementation
        USDHookBase.instructions = {
            "on_material_export": ["return False", "return True"],
            "on_export": ["throw", "return True"],
            "on_import": ["throw", "return True"],
        }

        USDHookBase.responses = {
            "on_material_export": [],
            "on_export": [],
            "on_import": [],
        }

        test_path = self.tempdir / "hook.usda"

        try:
            self.export_and_validate(filepath=str(test_path))
        except:
            pass

        try:
            bpy.ops.wm.usd_import(filepath=str(test_path))
        except:
            pass

        # Unregister the hooks. We do this here in case the following asserts fail.
        bpy.utils.unregister_class(USDHook1)
        bpy.utils.unregister_class(USDHook2)

        # Validate that the Hooks executed and responded accordingly...
        self.assertEqual(USDHookBase.responses["on_material_export"], ["returned False", "returned True"])
        self.assertEqual(USDHookBase.responses["on_export"], ["threw exception", "returned True"])
        self.assertEqual(USDHookBase.responses["on_import"], ["threw exception", "returned True"])

        # Now that the hooks are unregistered they should not be executed for import and export.
        USDHookBase.responses = {
            "on_material_export": [],
            "on_export": [],
            "on_import": [],
        }
        self.export_and_validate(filepath=str(test_path))
        self.export_and_validate(filepath=str(test_path))
        self.assertEqual(USDHookBase.responses["on_material_export"], [])
        self.assertEqual(USDHookBase.responses["on_export"], [])
        self.assertEqual(USDHookBase.responses["on_import"], [])

    def test_merge_parent_xform_false(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_hierarchy_export_test.blend"))

        test_path = self.tempdir / "test_merge_parent_xform_false.usda"

        self.export_and_validate(filepath=str(test_path), merge_parent_xform=False)

        expected = (
            ("/root", "Xform"),
            ("/root/Dupli1", "Xform"),
            ("/root/Dupli1/GEO_Head_0", "Xform"),
            ("/root/Dupli1/GEO_Head_0/Face", "Mesh"),
            ("/root/Dupli1/GEO_Head_0/GEO_Ear_R_2", "Xform"),
            ("/root/Dupli1/GEO_Head_0/GEO_Ear_R_2/Ear", "Mesh"),
            ("/root/Dupli1/GEO_Head_0/GEO_Ear_L_1", "Xform"),
            ("/root/Dupli1/GEO_Head_0/GEO_Ear_L_1/Ear", "Mesh"),
            ("/root/Dupli1/GEO_Head_0/GEO_Nose_3", "Xform"),
            ("/root/Dupli1/GEO_Head_0/GEO_Nose_3/Nose", "Mesh"),
            ("/root/_materials", "Scope"),
            ("/root/_materials/Head", "Material"),
            ("/root/_materials/Head/Principled_BSDF", "Shader"),
            ("/root/_materials/Nose", "Material"),
            ("/root/_materials/Nose/Principled_BSDF", "Shader"),
            ("/root/ParentOfDupli2", "Xform"),
            ("/root/ParentOfDupli2/Icosphere", "Mesh"),
            ("/root/ParentOfDupli2/Dupli2", "Xform"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0", "Xform"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0/Face", "Mesh"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0/GEO_Ear_L_1", "Xform"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0/GEO_Ear_L_1/Ear", "Mesh"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0/GEO_Ear_R_2", "Xform"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0/GEO_Ear_R_2/Ear", "Mesh"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0/GEO_Nose_3", "Xform"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0/GEO_Nose_3/Nose", "Mesh"),
            ("/root/Ground_plane", "Xform"),
            ("/root/Ground_plane/Plane", "Mesh"),
            ("/root/Ground_plane/OutsideDupliGrandParent", "Xform"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent", "Xform"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head", "Xform"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/Face", "Mesh"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_R", "Xform"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_R/Ear", "Mesh"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Nose", "Xform"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Nose/Nose", "Mesh"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_L", "Xform"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_L/Ear", "Mesh"),
            ("/root/Camera", "Xform"),
            ("/root/Camera/Camera", "Camera"),
            ("/root/env_light", "DomeLight")
        )

        def key(el):
            return el[0]

        expected = tuple(sorted(expected, key=key))

        stage = Usd.Stage.Open(str(test_path))
        actual = ((str(p.GetPath()), p.GetTypeName()) for p in stage.Traverse())
        actual = tuple(sorted(actual, key=key))

        self.assertTupleEqual(expected, actual)

    def test_merge_parent_xform_true(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_hierarchy_export_test.blend"))

        test_path = self.tempdir / "test_merge_parent_xform_true.usda"

        self.export_and_validate(filepath=str(test_path), merge_parent_xform=True)

        expected = (
            ("/root", "Xform"),
            ("/root/Dupli1", "Xform"),
            ("/root/Dupli1/GEO_Head_0", "Xform"),
            ("/root/Dupli1/GEO_Head_0/Face", "Mesh"),
            ("/root/Dupli1/GEO_Head_0/GEO_Ear_R_2", "Mesh"),
            ("/root/Dupli1/GEO_Head_0/GEO_Ear_L_1", "Mesh"),
            ("/root/Dupli1/GEO_Head_0/GEO_Nose_3", "Mesh"),
            ("/root/_materials", "Scope"),
            ("/root/_materials/Head", "Material"),
            ("/root/_materials/Head/Principled_BSDF", "Shader"),
            ("/root/_materials/Nose", "Material"),
            ("/root/_materials/Nose/Principled_BSDF", "Shader"),
            ("/root/ParentOfDupli2", "Xform"),
            ("/root/ParentOfDupli2/Icosphere", "Mesh"),
            ("/root/ParentOfDupli2/Dupli2", "Xform"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0", "Xform"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0/Face", "Mesh"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0/GEO_Ear_L_1", "Mesh"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0/GEO_Ear_R_2", "Mesh"),
            ("/root/ParentOfDupli2/Dupli2/GEO_Head_0/GEO_Nose_3", "Mesh"),
            ("/root/Ground_plane", "Xform"),
            ("/root/Ground_plane/Plane", "Mesh"),
            ("/root/Ground_plane/OutsideDupliGrandParent", "Xform"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent", "Xform"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head", "Xform"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/Face", "Mesh"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_R", "Mesh"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Nose", "Mesh"),
            ("/root/Ground_plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_L", "Mesh"),
            ("/root/Camera", "Camera"),
            ("/root/env_light", "DomeLight")
        )

        def key(el):
            return el[0]

        expected = tuple(sorted(expected, key=key))

        stage = Usd.Stage.Open(str(test_path))
        actual = ((str(p.GetPath()), p.GetTypeName()) for p in stage.Traverse())
        actual = tuple(sorted(actual, key=key))

        self.assertTupleEqual(expected, actual)

    def test_export_units(self):
        """Test specifying stage meters per unit on export."""
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))

        units = (
            ("mm", 'MILLIMETERS', 0.001), ("cm", 'CENTIMETERS', 0.01), ("km", 'KILOMETERS', 1000),
            ("in", 'INCHES', 0.0254), ("ft", 'FEET', 0.3048), ("yd", 'YARDS', 0.9144),
            ("default", "", 1), ("custom", 'CUSTOM', 0.125)
        )
        for name, unit, value in units:
            export_path = self.tempdir / f"usd_export_units_test_{name}.usda"
            if name == "default":
                self.export_and_validate(filepath=str(export_path))
            elif name == "custom":
                self.export_and_validate(filepath=str(export_path), convert_scene_units=unit, meters_per_unit=value)
            else:
                self.export_and_validate(filepath=str(export_path), convert_scene_units=unit)

            # Verify that meters per unit were set correctly
            stage = Usd.Stage.Open(str(export_path))
            self.assertEqual(UsdGeom.GetStageMetersPerUnit(stage), value)

    def test_export_native_instancing_true(self):
        """Test exporting instanced objects to native (scne graph) instances."""
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "nested_instancing_test.blend"))

        export_path = self.tempdir / "usd_export_nested_instancing_true.usda"
        self.export_and_validate(
            filepath=str(export_path),
            use_instancing=True
        )

        # The USD should contain two instances of a plane which has two
        # instances of a point cloud as children.
        stage = Usd.Stage.Open(str(export_path))

        stats = UsdUtils.ComputeUsdStageStats(stage)
        self.assertEqual(stats['totalInstanceCount'], 6, "Unexpected number of instances")
        self.assertEqual(stats['prototypeCount'], 2, "Unexpected number of prototypes")
        self.assertEqual(stats['primary']['primCountsByType']['Mesh'], 1, "Unexpected number of primary meshes")
        self.assertEqual(stats['primary']['primCountsByType']['Points'], 1, "Unexpected number of primary point clouds")
        self.assertEqual(stats['prototypes']['primCountsByType']['Mesh'], 1, "Unexpected number of prototype meshes")
        self.assertEqual(stats['prototypes']['primCountsByType']['Points'],
                         1, "Unexpected number of prototype point clouds")

        # Get the prototypes root.
        protos_root_path = Sdf.Path("/root/prototypes")
        prim = stage.GetPrimAtPath(protos_root_path)
        assert prim
        self.assertTrue(prim.IsAbstract())

        # Get the first plane instance.
        prim = stage.GetPrimAtPath("/root/plane_001/Plane_0")
        assert prim
        assert prim.IsInstance()

        # Get the second plane instance.
        prim = stage.GetPrimAtPath("/root/plane/Plane_0")
        assert prim
        assert prim.IsInstance()

        # Ensure all the prototype paths are under the pototypes root.
        for prim in stage.Traverse():
            if prim.IsInstance():
                arcs = Usd.PrimCompositionQuery.GetDirectReferences(prim).GetCompositionArcs()
                for arc in arcs:
                    target_path = arc.GetTargetPrimPath()
                    self.assertTrue(target_path.HasPrefix(protos_root_path))

    def test_export_native_instancing_false(self):
        """Test exporting instanced objects with instancing disabled."""
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "nested_instancing_test.blend"))

        export_path = self.tempdir / "usd_export_nested_instancing_false.usda"
        self.export_and_validate(
            filepath=str(export_path),
            use_instancing=False
        )

        # The USD should contain no instances.
        stage = Usd.Stage.Open(str(export_path))

        stats = UsdUtils.ComputeUsdStageStats(stage)
        self.assertEqual(stats['totalInstanceCount'], 0, "Unexpected number of instances")
        self.assertEqual(stats['prototypeCount'], 0, "Unexpected number of prototypes")
        self.assertEqual(stats['primary']['primCountsByType']['Mesh'], 2, "Unexpected number of primary meshes")
        self.assertEqual(stats['primary']['primCountsByType']['Points'], 4, "Unexpected number of primary point clouds")

    def test_texture_export_hook(self):
        """Exporting textures from on_material_export USD hook."""

        # Clear USD hook results.
        ExportTextureUSDHook.exported_textures = {}

        bpy.utils.register_class(ExportTextureUSDHook)
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_export.blend"))

        export_path = self.tempdir / "usd_materials_export.usda"

        self.export_and_validate(
            filepath=str(export_path),
            export_materials=True,
            generate_preview_surface=False,
        )

        # Verify that the exported texture paths were returned as expected.
        expected = {'/root/_materials/Transforms': './textures/test_grid_<UDIM>.png',
                    '/root/_materials/Clip_With_Round': './textures/test_grid_<UDIM>.png',
                    '/root/_materials/NormalMap': './textures/test_normal.exr',
                    '/root/_materials/Material': './textures/test_grid_<UDIM>.png',
                    '/root/_materials/Clip_With_LessThanInvert': './textures/test_grid_<UDIM>.png',
                    '/root/_materials/NormalMap_Scale_Bias': './textures/test_normal_invertY.exr'}

        self.assertDictEqual(ExportTextureUSDHook.exported_textures,
                             expected,
                             "Unexpected texture export paths")

        bpy.utils.unregister_class(ExportTextureUSDHook)

        # Verify that the texture files were copied as expected.
        tex_names = ['test_grid_1001.png', 'test_grid_1002.png',
                     'test_normal.exr', 'test_normal_invertY.exr']

        for name in tex_names:
            tex_path = self.tempdir / "textures" / name
            self.assertTrue(tex_path.exists(),
                            f"Exported texture {tex_path} doesn't exist")

    def test_inmem_pack_texture_export_hook(self):
        """Exporting packed and in memory textures from on_material_export USD hook."""

        # Clear hook results.
        ExportTextureUSDHook.exported_textures = {}

        bpy.utils.register_class(ExportTextureUSDHook)
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_inmem_pack.blend"))

        export_path = self.tempdir / "usd_materials_inmem_pack.usda"

        self.export_and_validate(
            filepath=str(export_path),
            export_materials=True,
            generate_preview_surface=False,
        )

        # Verify that the exported texture paths were returned as expected.
        expected = {'/root/_materials/MAT_pack_udim': './textures/test_grid_<UDIM>.png',
                    '/root/_materials/MAT_pack_single': './textures/test_single.png',
                    '/root/_materials/MAT_inmem_udim': './textures/inmem_udim.<UDIM>.png',
                    '/root/_materials/MAT_inmem_single': './textures/inmem_single.png'}

        self.assertDictEqual(ExportTextureUSDHook.exported_textures,
                             expected,
                             "Unexpected texture export paths")

        bpy.utils.unregister_class(ExportTextureUSDHook)

        # Verify that the texture files were copied as expected.
        tex_names = ['test_grid_1001.png', 'test_grid_1002.png',
                     'test_single.png',
                     'inmem_udim.1001.png', 'inmem_udim.1002.png',
                     'inmem_single.png']

        for name in tex_names:
            tex_path = self.tempdir / "textures" / name
            self.assertTrue(tex_path.exists(),
                            f"Exported texture {tex_path} doesn't exist")


class USDHookBase:
    instructions = {}
    responses = {}

    @staticmethod
    def follow_instructions(name, operation):
        instruction = USDHookBase.instructions[operation].pop(0)
        if instruction == "throw":
            USDHookBase.responses[operation].append("threw exception")
            raise RuntimeError(f"** {name} failing {operation} **")
        elif instruction == "return False":
            USDHookBase.responses[operation].append("returned False")
            return False

        USDHookBase.responses[operation].append("returned True")
        return True

    @staticmethod
    def do_on_export(name, export_context):
        stage = export_context.get_stage()
        depsgraph = export_context.get_depsgraph()
        if not stage.GetDefaultPrim().IsValid():
            raise RuntimeError("Unexpected failure: bad stage")
        if len(depsgraph.ids) == 0:
            raise RuntimeError("Unexpected failure: bad depsgraph")

        return USDHookBase.follow_instructions(name, "on_export")

    @staticmethod
    def do_on_material_export(name, export_context, bl_material, usd_material):
        stage = export_context.get_stage()
        if stage.expired:
            raise RuntimeError("Unexpected failure: bad stage")
        if not usd_material.GetPrim().IsValid():
            raise RuntimeError("Unexpected failure: bad usd_material")
        if bl_material is None:
            raise RuntimeError("Unexpected failure: bad bl_material")

        return USDHookBase.follow_instructions(name, "on_material_export")

    @staticmethod
    def do_on_import(name, import_context):
        stage = import_context.get_stage()
        if not stage.GetDefaultPrim().IsValid():
            raise RuntimeError("Unexpected failure: bad stage")

        return USDHookBase.follow_instructions(name, "on_import")


class USDHook1(USDHookBase, bpy.types.USDHook):
    bl_idname = "usd_hook_1"
    bl_label = "Hook 1"

    @staticmethod
    def on_export(export_context):
        return USDHookBase.do_on_export(USDHook1.bl_label, export_context)

    @staticmethod
    def on_material_export(export_context, bl_material, usd_material):
        return USDHookBase.do_on_material_export(USDHook1.bl_label, export_context, bl_material, usd_material)

    @staticmethod
    def on_import(import_context):
        return USDHookBase.do_on_import(USDHook1.bl_label, import_context)


class USDHook2(USDHookBase, bpy.types.USDHook):
    bl_idname = "usd_hook_2"
    bl_label = "Hook 2"

    @staticmethod
    def on_export(export_context):
        return USDHookBase.do_on_export(USDHook2.bl_label, export_context)

    @staticmethod
    def on_material_export(export_context, bl_material, usd_material):
        return USDHookBase.do_on_material_export(USDHook2.bl_label, export_context, bl_material, usd_material)

    @staticmethod
    def on_import(import_context):
        return USDHookBase.do_on_import(USDHook2.bl_label, import_context)


class ExportTextureUSDHook(bpy.types.USDHook):
    bl_idname = "export_texture_usd_hook"
    bl_label = "Export Texture USD Hook"

    exported_textures = {}

    @staticmethod
    def on_material_export(export_context, bl_material, usd_material):
        """
        If a texture image node exists in the given material's
        node tree, call exprt_texture() on the image and cache
        the returned path.
        """
        tex_image_node = None
        if bl_material and bl_material.node_tree:
            for node in bl_material.node_tree.nodes:
                if node.type == 'TEX_IMAGE':
                    tex_image_node = node

        if tex_image_node is None:
            return False

        tex_path = export_context.export_texture(tex_image_node.image)

        ExportTextureUSDHook.exported_textures[usd_material.GetPath()
                                               .pathString] = tex_path

        return True


def main():
    global args
    import argparse

    if "--" in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index("--") + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument("--testdir", required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining, verbosity=0)


if __name__ == "__main__":
    main()
