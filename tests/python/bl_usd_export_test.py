# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import pprint
import sys
import tempfile
import unittest
from pxr import Usd
from pxr import UsdUtils
from pxr import UsdGeom
from pxr import UsdShade
from pxr import Gf

import bpy

args = None


class AbstractUSDTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tempdir = tempfile.TemporaryDirectory()
        cls.testdir = args.testdir
        cls.tempdir = pathlib.Path(cls._tempdir.name)

        return cls

    def setUp(self):
        self.assertTrue(
            self.testdir.exists(), "Test dir {0} should exist".format(self.testdir)
        )

    def tearDown(self):
        self._tempdir.cleanup()


class USDExportTest(AbstractUSDTest):
    def test_export_usdchecker(self):
        """Test exporting a scene and verifying it passes the usdchecker test suite"""
        bpy.ops.wm.open_mainfile(
            filepath=str(self.testdir / "usd_materials_export.blend")
        )
        export_path = self.tempdir / "usdchecker.usda"
        res = bpy.ops.wm.usd_export(
            filepath=str(export_path),
            export_materials=True,
            evaluation_mode="RENDER",
        )
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {export_path}")

        checker = UsdUtils.ComplianceChecker(
            arkit=False,
            skipARKitRootLayerCheck=False,
            rootPackageOnly=False,
            skipVariants=False,
            verbose=False,
        )
        checker.CheckCompliance(str(export_path))

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

    def compareVec3d(self, first, second):
        places = 5
        self.assertAlmostEqual(first[0], second[0], places)
        self.assertAlmostEqual(first[1], second[1], places)
        self.assertAlmostEqual(first[2], second[2], places)

    def test_export_extents(self):
        """Test that exported scenes contain have a properly authored extent attribute on each boundable prim"""
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_extent_test.blend"))
        export_path = self.tempdir / "usd_extent_test.usda"
        res = bpy.ops.wm.usd_export(
            filepath=str(export_path),
            export_materials=True,
            evaluation_mode="RENDER",
            convert_world_material=False,
        )
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {export_path}")

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

    def test_opacity_threshold(self):
        # Note that the scene file used here is shared with a different test.
        # Here we assume that it has a Principled BSDF material with
        # a texture connected to its Base Color input.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "usd_materials_export.blend"))

        export_path = self.tempdir / "opaque_material.usda"
        res = bpy.ops.wm.usd_export(
            filepath=str(export_path),
            export_materials=True,
            evaluation_mode="RENDER",
        )
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {export_path}")

        # Inspect and validate the exported USD for the opaque blend case.
        stage = Usd.Stage.Open(str(export_path))
        shader_prim = stage.GetPrimAtPath("/root/_materials/Material/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        opacity_input = shader.GetInput('opacity')
        self.assertEqual(opacity_input.HasConnectedSource(), False,
                         "Opacity input should not be connected for opaque material")
        self.assertAlmostEqual(opacity_input.Get(), 1.0, 2, "Opacity input should be set to 1")

        # Inspect and validate the exported USD for the alpha clip w/Round node case.
        shader_prim = stage.GetPrimAtPath("/root/_materials/Clip_With_Round/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        opacity_input = shader.GetInput('opacity')
        opacity_thresh_input = shader.GetInput('opacityThreshold')
        self.assertEqual(opacity_input.HasConnectedSource(), True, "Alpha input should be connected")
        self.assertAlmostEqual(opacity_thresh_input.Get(), 0.5, 2, "Opacity threshold input should be 0.5")

        # Inspect and validate the exported USD for the alpha clip w/LessThan+Invert node case.
        shader_prim = stage.GetPrimAtPath("/root/_materials/Clip_With_LessThanInvert/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        opacity_input = shader.GetInput('opacity')
        opacity_thresh_input = shader.GetInput('opacityThreshold')
        self.assertEqual(opacity_input.HasConnectedSource(), True, "Alpha input should be connected")
        self.assertAlmostEqual(opacity_thresh_input.Get(), 0.2, 2, "Opacity threshold input should be 0.2")

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
        res = bpy.ops.wm.usd_export(filepath=str(export_path), evaluation_mode="RENDER")
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {export_path}")

        stage = Usd.Stage.Open(str(export_path))

        # Validate all expected Mesh attributes. Notice that nothing on
        # the Edge domain is supported by USD.
        prim = stage.GetPrimAtPath("/root/Mesh/Mesh")

        self.check_primvar(prim, "p_bool", "VtArray<bool>", "vertex", 4)
        self.check_primvar(prim, "p_int8", "VtArray<int>", "vertex", 4)
        self.check_primvar(prim, "p_int32", "VtArray<int>", "vertex", 4)
        self.check_primvar(prim, "p_float", "VtArray<float>", "vertex", 4)
        self.check_primvar(prim, "p_color", "VtArray<GfVec3f>", "vertex", 4)
        self.check_primvar(prim, "p_byte_color", "VtArray<GfVec3f>", "vertex", 4)
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
        self.check_primvar(prim, "f_int8", "VtArray<int>", "uniform", 1)
        self.check_primvar(prim, "f_int32", "VtArray<int>", "uniform", 1)
        self.check_primvar(prim, "f_float", "VtArray<float>", "uniform", 1)
        self.check_primvar_missing(prim, "f_color")
        self.check_primvar_missing(prim, "f_byte_color")
        self.check_primvar(prim, "f_vec2", "VtArray<GfVec2f>", "uniform", 1)
        self.check_primvar(prim, "f_vec3", "VtArray<GfVec3f>", "uniform", 1)
        self.check_primvar(prim, "f_quat", "VtArray<GfQuatf>", "uniform", 1)
        self.check_primvar_missing(prim, "f_mat4x4")

        self.check_primvar(prim, "fc_bool", "VtArray<bool>", "faceVarying", 4)
        self.check_primvar(prim, "fc_int8", "VtArray<int>", "faceVarying", 4)
        self.check_primvar(prim, "fc_int32", "VtArray<int>", "faceVarying", 4)
        self.check_primvar(prim, "fc_float", "VtArray<float>", "faceVarying", 4)
        self.check_primvar(prim, "fc_color", "VtArray<GfVec3f>", "faceVarying", 4)
        self.check_primvar(prim, "fc_byte_color", "VtArray<GfVec3f>", "faceVarying", 4)
        self.check_primvar(prim, "fc_vec2", "VtArray<GfVec2f>", "faceVarying", 4)
        self.check_primvar(prim, "fc_vec3", "VtArray<GfVec3f>", "faceVarying", 4)
        self.check_primvar(prim, "fc_quat", "VtArray<GfQuatf>", "faceVarying", 4)
        self.check_primvar_missing(prim, "fc_mat4x4")

        prim = stage.GetPrimAtPath("/root/Curve_base/Curves/Curves")

        self.check_primvar(prim, "p_bool", "VtArray<bool>", "vertex", 24)
        self.check_primvar(prim, "p_int8", "VtArray<int>", "vertex", 24)
        self.check_primvar(prim, "p_int32", "VtArray<int>", "vertex", 24)
        self.check_primvar(prim, "p_float", "VtArray<float>", "vertex", 24)
        self.check_primvar_missing(prim, "p_color")
        self.check_primvar_missing(prim, "p_byte_color")
        self.check_primvar(prim, "p_vec2", "VtArray<GfVec2f>", "vertex", 24)
        self.check_primvar(prim, "p_vec3", "VtArray<GfVec3f>", "vertex", 24)
        self.check_primvar(prim, "p_quat", "VtArray<GfQuatf>", "vertex", 24)
        self.check_primvar_missing(prim, "p_mat4x4")

        self.check_primvar(prim, "sp_bool", "VtArray<bool>", "uniform", 2)
        self.check_primvar(prim, "sp_int8", "VtArray<int>", "uniform", 2)
        self.check_primvar(prim, "sp_int32", "VtArray<int>", "uniform", 2)
        self.check_primvar(prim, "sp_float", "VtArray<float>", "uniform", 2)
        self.check_primvar_missing(prim, "sp_color")
        self.check_primvar_missing(prim, "sp_byte_color")
        self.check_primvar(prim, "sp_vec2", "VtArray<GfVec2f>", "uniform", 2)
        self.check_primvar(prim, "sp_vec3", "VtArray<GfVec3f>", "uniform", 2)
        self.check_primvar(prim, "sp_quat", "VtArray<GfQuatf>", "uniform", 2)
        self.check_primvar_missing(prim, "sp_mat4x4")

        prim = stage.GetPrimAtPath("/root/Curve_bezier_base/Curves_bezier/Curves")

        self.check_primvar(prim, "p_bool", "VtArray<bool>", "varying", 10)
        self.check_primvar(prim, "p_int8", "VtArray<int>", "varying", 10)
        self.check_primvar(prim, "p_int32", "VtArray<int>", "varying", 10)
        self.check_primvar(prim, "p_float", "VtArray<float>", "varying", 10)
        self.check_primvar_missing(prim, "p_color")
        self.check_primvar_missing(prim, "p_byte_color")
        self.check_primvar(prim, "p_vec2", "VtArray<GfVec2f>", "varying", 10)
        self.check_primvar(prim, "p_vec3", "VtArray<GfVec3f>", "varying", 10)
        self.check_primvar(prim, "p_quat", "VtArray<GfQuatf>", "varying", 10)
        self.check_primvar_missing(prim, "p_mat4x4")

        self.check_primvar(prim, "sp_bool", "VtArray<bool>", "uniform", 3)
        self.check_primvar(prim, "sp_int8", "VtArray<int>", "uniform", 3)
        self.check_primvar(prim, "sp_int32", "VtArray<int>", "uniform", 3)
        self.check_primvar(prim, "sp_float", "VtArray<float>", "uniform", 3)
        self.check_primvar_missing(prim, "sp_color")
        self.check_primvar_missing(prim, "sp_byte_color")
        self.check_primvar(prim, "sp_vec2", "VtArray<GfVec2f>", "uniform", 3)
        self.check_primvar(prim, "sp_vec3", "VtArray<GfVec3f>", "uniform", 3)
        self.check_primvar(prim, "sp_quat", "VtArray<GfQuatf>", "uniform", 3)
        self.check_primvar_missing(prim, "sp_mat4x4")


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

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
