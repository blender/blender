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
        )
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {export_path}")

        # if prims are missing, the exporter must have skipped some objects
        stats = UsdUtils.ComputeUsdStageStats(str(export_path))
        self.assertEqual(stats["totalPrimCount"], 15, "Unexpected number of prims")

        # validate the overall world bounds of the scene
        stage = Usd.Stage.Open(str(export_path))
        scenePrim = stage.GetPrimAtPath("/scene")
        bboxcache = UsdGeom.BBoxCache(Usd.TimeCode.Default(), [UsdGeom.Tokens.default_])
        bounds = bboxcache.ComputeWorldBound(scenePrim)
        bound_min = bounds.GetRange().GetMin()
        bound_max = bounds.GetRange().GetMax()
        self.compareVec3d(bound_min, Gf.Vec3d(-5.752975881, -1, -2.798513651))
        self.compareVec3d(bound_max, Gf.Vec3d(1, 2.9515805244, 2.7985136508))

        # validate the locally authored extents
        prim = stage.GetPrimAtPath("/scene/BigCube/BigCubeMesh")
        extent = UsdGeom.Boundable(prim).GetExtentAttr().Get()
        self.compareVec3d(Gf.Vec3d(extent[0]), Gf.Vec3d(-1, -1, -2.7985137))
        self.compareVec3d(Gf.Vec3d(extent[1]), Gf.Vec3d(1, 1, 2.7985137))
        prim = stage.GetPrimAtPath("/scene/LittleCube/LittleCubeMesh")
        extent = UsdGeom.Boundable(prim).GetExtentAttr().Get()
        self.compareVec3d(Gf.Vec3d(extent[0]), Gf.Vec3d(-1, -1, -1))
        self.compareVec3d(Gf.Vec3d(extent[1]), Gf.Vec3d(1, 1, 1))
        prim = stage.GetPrimAtPath("/scene/Volume/Volume")
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
        shader_prim = stage.GetPrimAtPath("/_materials/Material/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        opacity_input = shader.GetInput('opacity')
        self.assertEqual(opacity_input.HasConnectedSource(), False,
                         "Opacity input should not be connected for opaque material")
        self.assertAlmostEqual(opacity_input.Get(), 1.0, "Opacity input should be set to 1")

        # The material already has a texture input to the Base Color.
        # Now also link this texture to the Alpha input.
        # Set an opacity threshold appropriate for alpha clipping.
        mat = bpy.data.materials['Material']
        bsdf = mat.node_tree.nodes['Principled BSDF']
        tex_output = bsdf.inputs['Base Color'].links[0].from_node.outputs['Color']
        alpha_input = bsdf.inputs['Alpha']
        mat.node_tree.links.new(tex_output, alpha_input)
        bpy.data.materials['Material'].blend_method = 'CLIP'
        bpy.data.materials['Material'].alpha_threshold = 0.01
        export_path = self.tempdir / "alphaclip_material.usda"
        res = bpy.ops.wm.usd_export(
            filepath=str(export_path),
            export_materials=True,
            evaluation_mode="RENDER",
        )
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {export_path}")

        # Inspect and validate the exported USD for the alpha clip case.
        stage = Usd.Stage.Open(str(export_path))
        shader_prim = stage.GetPrimAtPath("/_materials/Material/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        opacity_input = shader.GetInput('opacity')
        opacity_thres_input = shader.GetInput('opacityThreshold')
        self.assertEqual(opacity_input.HasConnectedSource(), True, "Alpha input should be connected")
        self.assertGreater(opacity_thres_input.Get(), 0.0, "Opacity threshold input should be > 0")

        # Modify material again, this time with alpha blend.
        bpy.data.materials['Material'].blend_method = 'BLEND'
        export_path = self.tempdir / "alphablend_material.usda"
        res = bpy.ops.wm.usd_export(
            filepath=str(export_path),
            export_materials=True,
            evaluation_mode="RENDER",
        )
        self.assertEqual({'FINISHED'}, res, f"Unable to export to {export_path}")

        # Inspect and validate the exported USD for the alpha blend case.
        stage = Usd.Stage.Open(str(export_path))
        shader_prim = stage.GetPrimAtPath("/_materials/Material/Principled_BSDF")
        shader = UsdShade.Shader(shader_prim)
        opacity_input = shader.GetInput('opacity')
        opacity_thres_input = shader.GetInput('opacityThreshold')
        self.assertEqual(opacity_input.HasConnectedSource(), True, "Alpha input should be connected")
        self.assertEqual(opacity_thres_input.Get(), None, "Opacity threshold should not be specified for alpha blend")


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
