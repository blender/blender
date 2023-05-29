# SPDX-License-Identifier: GPL-2.0-or-later

"""
blender -b -noaudio --factory-startup --python tests/python/bl_animation_fcurves.py -- --testdir /path/to/lib/tests/animation
"""

import pathlib
import sys
import unittest
from math import degrees, radians
from typing import List

import bpy


class AbstractAnimationTest:
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        'Test dir %s should exist' % self.testdir)


class FCurveEvaluationTest(AbstractAnimationTest, unittest.TestCase):
    def test_fcurve_versioning_291(self):
        # See D8752.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "fcurve-versioning-291.blend"))
        cube = bpy.data.objects['Cube']
        fcurve = cube.animation_data.action.fcurves.find('location', index=0)

        self.assertAlmostEqual(0.0, fcurve.evaluate(1))
        self.assertAlmostEqual(0.019638698548078537, fcurve.evaluate(2))
        self.assertAlmostEqual(0.0878235399723053, fcurve.evaluate(3))
        self.assertAlmostEqual(0.21927043795585632, fcurve.evaluate(4))
        self.assertAlmostEqual(0.41515052318573, fcurve.evaluate(5))
        self.assertAlmostEqual(0.6332430243492126, fcurve.evaluate(6))
        self.assertAlmostEqual(0.8106040954589844, fcurve.evaluate(7))
        self.assertAlmostEqual(0.924369215965271, fcurve.evaluate(8))
        self.assertAlmostEqual(0.9830065965652466, fcurve.evaluate(9))
        self.assertAlmostEqual(1.0, fcurve.evaluate(10))

    def test_fcurve_extreme_handles(self):
        # See D8752.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "fcurve-extreme-handles.blend"))
        cube = bpy.data.objects['Cube']
        fcurve = cube.animation_data.action.fcurves.find('location', index=0)

        self.assertAlmostEqual(0.0, fcurve.evaluate(1))
        self.assertAlmostEqual(0.004713400732725859, fcurve.evaluate(2))
        self.assertAlmostEqual(0.022335870191454887, fcurve.evaluate(3))
        self.assertAlmostEqual(0.06331237405538559, fcurve.evaluate(4))
        self.assertAlmostEqual(0.16721539199352264, fcurve.evaluate(5))
        self.assertAlmostEqual(0.8327845335006714, fcurve.evaluate(6))
        self.assertAlmostEqual(0.9366875886917114, fcurve.evaluate(7))
        self.assertAlmostEqual(0.9776642322540283, fcurve.evaluate(8))
        self.assertAlmostEqual(0.9952865839004517, fcurve.evaluate(9))
        self.assertAlmostEqual(1.0, fcurve.evaluate(10))


class EulerFilterTest(AbstractAnimationTest, unittest.TestCase):
    def setUp(self):
        super().setUp()
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "euler-filter.blend"))

    def test_multi_channel_filter(self):
        """Test fixing discontinuities that require all X/Y/Z rotations to work."""

        self.activate_object('Three-Channel-Jump')
        fcu_rot = self.active_object_rotation_channels()

        # # Check some pre-filter values to make sure the file is as we expect.
        # Keyframes before the "jump". These shouldn't be touched by the filter.
        self.assertEqualAngle(-87.5742, fcu_rot[0], 22)
        self.assertEqualAngle(69.1701, fcu_rot[1], 22)
        self.assertEqualAngle(-92.3918, fcu_rot[2], 22)
        # Keyframes after the "jump". These should be updated by the filter.
        self.assertEqualAngle(81.3266, fcu_rot[0], 23)
        self.assertEqualAngle(111.422, fcu_rot[1], 23)
        self.assertEqualAngle(76.5996, fcu_rot[2], 23)

        with bpy.context.temp_override(**self.get_context()):
            bpy.ops.graph.euler_filter()

        # Keyframes before the "jump". These shouldn't be touched by the filter.
        self.assertEqualAngle(-87.5742, fcu_rot[0], 22)
        self.assertEqualAngle(69.1701, fcu_rot[1], 22)
        self.assertEqualAngle(-92.3918, fcu_rot[2], 22)
        # Keyframes after the "jump". These should be updated by the filter.
        self.assertEqualAngle(-98.6734, fcu_rot[0], 23)
        self.assertEqualAngle(68.5783, fcu_rot[1], 23)
        self.assertEqualAngle(-103.4, fcu_rot[2], 23)

    def test_single_channel_filter(self):
        """Test fixing discontinuities in single channels."""

        self.activate_object('One-Channel-Jumps')
        fcu_rot = self.active_object_rotation_channels()

        # # Check some pre-filter values to make sure the file is as we expect.
        # Keyframes before the "jump". These shouldn't be touched by the filter.
        self.assertEqualAngle(360, fcu_rot[0], 15)
        self.assertEqualAngle(396, fcu_rot[1], 21)  # X and Y are keyed on different frames.
        # Keyframes after the "jump". These should be updated by the filter.
        self.assertEqualAngle(720, fcu_rot[0], 16)
        self.assertEqualAngle(72, fcu_rot[1], 22)

        with bpy.context.temp_override(**self.get_context()):
            bpy.ops.graph.euler_filter()

        # Keyframes before the "jump". These shouldn't be touched by the filter.
        self.assertEqualAngle(360, fcu_rot[0], 15)
        self.assertEqualAngle(396, fcu_rot[1], 21)  # X and Y are keyed on different frames.
        # Keyframes after the "jump". These should be updated by the filter.
        self.assertEqualAngle(360, fcu_rot[0], 16)
        self.assertEqualAngle(432, fcu_rot[1], 22)

    def assertEqualAngle(self, angle_degrees: float, fcurve: bpy.types.FCurve, frame: int) -> None:
        self.assertAlmostEqual(
            radians(angle_degrees),
            fcurve.evaluate(frame),
            4,
            "Expected %.3f degrees, but FCurve %s[%d] evaluated to %.3f on frame %d" % (
                angle_degrees, fcurve.data_path, fcurve.array_index, degrees(fcurve.evaluate(frame)), frame,
            )
        )

    @staticmethod
    def get_context():
        ctx = bpy.context.copy()

        for area in bpy.context.window.screen.areas:
            if area.type != 'GRAPH_EDITOR':
                continue

            ctx['area'] = area
            ctx['space'] = area.spaces.active
            break

        return ctx

    @staticmethod
    def activate_object(object_name: str) -> None:
        ob = bpy.data.objects[object_name]
        bpy.context.view_layer.objects.active = ob

    @staticmethod
    def active_object_rotation_channels() -> List[bpy.types.FCurve]:
        ob = bpy.context.view_layer.objects.active
        action = ob.animation_data.action
        return [action.fcurves.find('rotation_euler', index=idx) for idx in range(3)]


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
