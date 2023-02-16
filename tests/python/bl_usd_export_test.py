# SPDX-License-Identifier: GPL-2.0-or-later
import enum
import pathlib
import pprint
import sys
import tempfile
import unittest
from pxr import Usd
from pxr import UsdUtils
from pxr import UsdGeom
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
