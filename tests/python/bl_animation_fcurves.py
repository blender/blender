# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
blender -b --factory-startup --python tests/python/bl_animation_fcurves.py -- --testdir tests/files/animation
"""

import pathlib
import sys
import unittest
from math import degrees, radians

import bpy


class AbstractAnimationTest:
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir

    def setUp(self):
        assert isinstance(self, unittest.TestCase)
        self.assertTrue(self.testdir.exists(),
                        'Test dir %s should exist' % self.testdir)


def _channelbag(animated_id: bpy.types.ID) -> bpy.types.ActionChannelbag:
    """Return the first layer's Channelbag of the animated ID's Action."""
    action = animated_id.animation_data.action
    action_slot = animated_id.animation_data.action_slot
    channelbag = action.layers[0].strips[0].channelbag(action_slot)
    assert channelbag is not None
    return channelbag


def _first_fcurve(animated_id: bpy.types.ID) -> bpy.types.FCurve:
    """Return the first F-Curve of the animated ID's Action."""
    return _channelbag(animated_id).fcurves[0]


class FCurveEvaluationTest(AbstractAnimationTest, unittest.TestCase):
    def test_fcurve_versioning_291(self):
        # See D8752.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "fcurve-versioning-291.blend"))
        cube = bpy.data.objects['Cube']
        channelbag = cube.animation_data.action.layers[0].strips[0].channelbags[0]
        fcurve = channelbag.fcurves.find('location', index=0)

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
        channelbag = cube.animation_data.action.layers[0].strips[0].channelbags[0]
        fcurve = channelbag.fcurves.find('location', index=0)

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


class PropertyInterpolationTest(AbstractAnimationTest, unittest.TestCase):
    """Test F-Curve interpolation on RNA properties.

    This tests both the evaluation of the RNA property and the F-Curve
    interpolation itself (the not-exposed-to-RNA flags `FCURVE_INT_VALUES` and
    `FCURVE_DISCRETE_VALUES` have an impact on the latter as well).
    """

    def setUp(self):
        bpy.ops.wm.read_homefile(use_factory_startup=True)

    def test_float(self) -> None:
        scene: bpy.types.Scene = bpy.context.scene

        camera_ob: bpy.types.Object = scene.objects["Camera"]
        camera: bpy.types.Camera = camera_ob.data
        camera.lens = 16
        camera.keyframe_insert('lens', frame=0)
        camera.lens = 32
        camera.keyframe_insert('lens', frame=64)

        self._make_all_keys_linear()
        fcurve = _first_fcurve(camera)

        scene.frame_set(0)
        self.assertAlmostEqual(16, camera.lens)
        self.assertAlmostEqual(16, fcurve.evaluate(0))
        scene.frame_set(1)
        self.assertAlmostEqual(16.25, camera.lens)
        self.assertAlmostEqual(16.25, fcurve.evaluate(1))
        scene.frame_set(2)
        self.assertAlmostEqual(16.5, camera.lens)
        self.assertAlmostEqual(16.5, fcurve.evaluate(2))
        scene.frame_set(11)
        self.assertAlmostEqual(18.75, camera.lens)
        self.assertAlmostEqual(18.75, fcurve.evaluate(11))
        scene.frame_set(64)
        self.assertAlmostEqual(32, camera.lens)
        self.assertAlmostEqual(32, fcurve.evaluate(64))

    def test_int(self) -> None:
        scene: bpy.types.Scene = bpy.context.scene
        render: bpy.types.RenderSettings = scene.render

        render.simplify_subdivision = 16
        render.keyframe_insert('simplify_subdivision', frame=0)
        render.simplify_subdivision = 32
        render.keyframe_insert('simplify_subdivision', frame=64)

        self._make_all_keys_linear()
        fcurve = _first_fcurve(scene)

        scene.frame_set(0)
        self.assertAlmostEqual(16, render.simplify_subdivision)
        self.assertAlmostEqual(16, fcurve.evaluate(0))
        scene.frame_set(1)  # 16.25 rounds down.
        self.assertAlmostEqual(16, render.simplify_subdivision)
        self.assertAlmostEqual(16, fcurve.evaluate(1))
        scene.frame_set(2)  # 16.50 rounds up.
        self.assertAlmostEqual(17, render.simplify_subdivision)
        self.assertAlmostEqual(17, fcurve.evaluate(2))
        scene.frame_set(11)  # 18.75 rounds up.
        self.assertAlmostEqual(19, render.simplify_subdivision)
        self.assertAlmostEqual(19, fcurve.evaluate(11))
        scene.frame_set(64)
        self.assertAlmostEqual(32, render.simplify_subdivision)
        self.assertAlmostEqual(32, fcurve.evaluate(64))

    def test_bool(self) -> None:
        scene: bpy.types.Scene = bpy.context.scene
        render: bpy.types.RenderSettings = scene.render

        render.use_simplify = False
        render.keyframe_insert('use_simplify', frame=0)
        render.use_simplify = True
        render.keyframe_insert('use_simplify', frame=64)

        self._make_all_keys_linear()
        fcurve = _first_fcurve(scene)

        scene.frame_set(0)
        self.assertEqual(False, render.use_simplify)
        self.assertAlmostEqual(0, fcurve.evaluate(0))
        scene.frame_set(32)  # Boolean F-Curves should not interpolate at all.
        self.assertEqual(False, render.use_simplify)
        self.assertAlmostEqual(0, fcurve.evaluate(63))
        scene.frame_set(63)  # Should remain False until the frame it goes to True.
        self.assertEqual(False, render.use_simplify)
        self.assertAlmostEqual(0, fcurve.evaluate(63))
        scene.frame_set(64)
        self.assertEqual(True, render.use_simplify)
        self.assertAlmostEqual(1, fcurve.evaluate(64))

    def test_enum(self) -> None:
        scene: bpy.types.Scene = bpy.context.scene
        cube: bpy.types.Object = scene.objects["Cube"]

        cube.rotation_mode = 'QUATERNION'  # First item in the enum.
        cube.keyframe_insert('rotation_mode', frame=0)
        cube.rotation_mode = 'ZYX'  # Item in the enum with the highest value.
        # Yes, 'AXIS_ANGLE' is the last one in the enum, but that has value -1
        # for historical reasons, and so for this test it's a bit weird.
        cube.keyframe_insert('rotation_mode', frame=64)

        self._make_all_keys_linear()
        fcurve = _first_fcurve(cube)

        scene.frame_set(0)
        self.assertEqual('QUATERNION', cube.rotation_mode)
        self.assertAlmostEqual(0, fcurve.evaluate(0))
        scene.frame_set(32)  # Enum F-Curves should not interpolate at all.
        self.assertEqual('QUATERNION', cube.rotation_mode)
        scene.frame_set(63)  # Should remain 'QUATERNION' until the frame it goes to another value.
        self.assertEqual('QUATERNION', cube.rotation_mode)
        self.assertAlmostEqual(0, fcurve.evaluate(63))
        scene.frame_set(64)
        self.assertEqual('ZYX', cube.rotation_mode)
        self.assertAlmostEqual(6, fcurve.evaluate(64))

    def _make_all_keys_linear(self) -> None:
        """Make all keys in all Actions linearly interpolated.

        This makes the code in this test a bit simpler, and shouldn't have any
        effect on the actual mapping of the F-Curve value to the property value.
        """

        for action in bpy.data.actions:
            for layer in action.layers:
                for strip in layer.strips:
                    self.assertEqual(strip.type, 'KEYFRAME')
                    for channelbag in strip.channelbags:
                        for fcurve in channelbag.fcurves:
                            for key in fcurve.keyframe_points:
                                key.interpolation = 'LINEAR'


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
    def active_object_rotation_channels() -> list[bpy.types.FCurve]:
        ob = bpy.context.view_layer.objects.active
        channelbag = _channelbag(ob)
        return [channelbag.fcurves.find('rotation_euler', index=idx) for idx in range(3)]


def get_view3d_context():
    ctx = bpy.context.copy()

    for area in bpy.context.window.screen.areas:
        if area.type != 'VIEW_3D':
            continue

        ctx['area'] = area
        ctx['space'] = area.spaces.active
        break

    return ctx


class KeyframeInsertTest(AbstractAnimationTest, unittest.TestCase):
    def setUp(self):
        super().setUp()
        bpy.ops.wm.read_homefile(use_factory_startup=True)

    def test_keyframe_insertion_basic(self):
        bpy.ops.mesh.primitive_monkey_add()
        key_count = 100
        with bpy.context.temp_override(**get_view3d_context()):
            for frame in range(key_count):
                bpy.context.scene.frame_set(frame)
                bpy.ops.anim.keyframe_insert_by_name(type="Location")

        key_object = bpy.context.active_object
        fcurve = _first_fcurve(key_object)
        for key_index in range(key_count):
            key = fcurve.keyframe_points[key_index]
            self.assertEqual(key.co.x, key_index)

        bpy.ops.object.delete(use_global=False)

    def test_keyframe_insert_keytype(self):
        key_object = bpy.context.active_object

        # Inserting a key with a specific type should work.
        key_object.keyframe_insert("location", keytype='GENERATED')

        # Unsupported/unknown types should be rejected.
        with self.assertRaises(ValueError):
            key_object.keyframe_insert("rotation_euler", keytype='UNSUPPORTED')

        # Only a single key should have been inserted.
        keys = _first_fcurve(key_object).keyframe_points
        self.assertEqual(len(keys), 1)
        self.assertEqual(keys[0].type, 'GENERATED')

    def test_keyframe_insertion_high_frame_number(self):
        bpy.ops.mesh.primitive_monkey_add()
        key_count = 100
        frame_offset = 1000000
        with bpy.context.temp_override(**get_view3d_context()):
            for frame in range(key_count):
                bpy.context.scene.frame_set(frame + frame_offset)
                bpy.ops.anim.keyframe_insert_by_name(type="Location")

        key_object = bpy.context.active_object
        for key_index in range(key_count):
            key = _first_fcurve(key_object).keyframe_points[key_index]
            self.assertEqual(key.co.x, key_index + frame_offset)

        bpy.ops.object.delete(use_global=False)

    def test_keyframe_insertion_subframes_basic(self):
        bpy.ops.mesh.primitive_monkey_add()
        key_count = 50
        with bpy.context.temp_override(**get_view3d_context()):
            for i in range(key_count):
                bpy.context.scene.frame_set(0, subframe=i / key_count)
                bpy.ops.anim.keyframe_insert_by_name(type="Location")

        key_object = bpy.context.active_object
        for key_index in range(key_count):
            key = _first_fcurve(key_object).keyframe_points[key_index]
            self.assertAlmostEqual(key.co.x, key_index / key_count)

        bpy.ops.object.delete(use_global=False)

    def test_keyframe_insertion_subframes_high_frame_number(self):
        bpy.ops.mesh.primitive_monkey_add()
        key_count = 50
        frame_offset = 1000000
        with bpy.context.temp_override(**get_view3d_context()):
            for i in range(key_count):
                bpy.context.scene.frame_set(frame_offset, subframe=i / key_count)
                bpy.ops.anim.keyframe_insert_by_name(type="Location")

        key_object = bpy.context.active_object
        # These are the possible floating point steps from "1.000.000" up to "1.000.001".
        floating_point_steps = [
            1000000.0,
            1000000.0625,
            1000000.125,
            1000000.1875,
            1000000.25,
            1000000.3125,
            1000000.375,
            1000000.4375,
            1000000.5,
            1000000.5625,
            1000000.625,
            1000000.6875,
            1000000.75,
            1000000.8125,
            1000000.875,
            1000000.9375,
            # Even though range() is exclusive, the floating point limitations mean keys end up on that position.
            1000001.0
        ]
        keyframe_points = _first_fcurve(key_object).keyframe_points
        for i, value in enumerate(floating_point_steps):
            key = keyframe_points[i]
            self.assertAlmostEqual(key.co.x, value)

        # This checks that there is a key on every possible floating point value and not more than that.
        self.assertEqual(len(floating_point_steps), len(keyframe_points))

        bpy.ops.object.delete(use_global=False)


class KeyframeDeleteTest(AbstractAnimationTest, unittest.TestCase):
    def setUp(self):
        super().setUp()
        bpy.ops.wm.read_homefile(use_factory_startup=True)

    def test_keyframe_deletion_basic(self):
        bpy.ops.mesh.primitive_monkey_add()
        key_count = 100
        with bpy.context.temp_override(**get_view3d_context()):
            bpy.context.scene.frame_set(-1)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")

        key_object = bpy.context.active_object
        fcu = _first_fcurve(key_object)
        for i in range(key_count):
            fcu.keyframe_points.insert(frame=i, value=0)

        with bpy.context.temp_override(**get_view3d_context()):
            for frame in range(key_count):
                bpy.context.scene.frame_set(frame)
                bpy.ops.anim.keyframe_delete_by_name(type="Location")

        # Only the key on frame -1 should be left
        self.assertEqual(len(fcu.keyframe_points), 1)

        bpy.ops.object.delete(use_global=False)

    def test_keyframe_deletion_high_frame_number(self):
        bpy.ops.mesh.primitive_monkey_add()
        key_count = 100
        frame_offset = 1000000
        with bpy.context.temp_override(**get_view3d_context()):
            bpy.context.scene.frame_set(-1)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")

        key_object = bpy.context.active_object
        fcu = _first_fcurve(key_object)
        for i in range(key_count):
            fcu.keyframe_points.insert(frame=i + frame_offset, value=0)

        with bpy.context.temp_override(**get_view3d_context()):
            for frame in range(key_count):
                bpy.context.scene.frame_set(frame + frame_offset)
                bpy.ops.anim.keyframe_delete_by_name(type="Location")

        # Only the key on frame -1 should be left
        self.assertEqual(len(fcu.keyframe_points), 1)

        bpy.ops.object.delete(use_global=False)

    def test_keyframe_deletion_subframe_basic(self):
        bpy.ops.mesh.primitive_monkey_add()
        key_count = 50
        with bpy.context.temp_override(**get_view3d_context()):
            bpy.context.scene.frame_set(-1)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")

        key_object = bpy.context.active_object
        fcu = _first_fcurve(key_object)
        for i in range(key_count):
            fcu.keyframe_points.insert(frame=i / key_count, value=0)

        with bpy.context.temp_override(**get_view3d_context()):
            for frame in range(key_count):
                bpy.context.scene.frame_set(0, subframe=frame / key_count)
                bpy.ops.anim.keyframe_delete_by_name(type="Location")

        # Only the key on frame -1 should be left
        self.assertEqual(len(fcu.keyframe_points), 1)

        bpy.ops.object.delete(use_global=False)

    def test_keyframe_deletion_subframe_high_frame_number(self):
        bpy.ops.mesh.primitive_monkey_add()
        key_count = 50
        frame_offset = 1000000
        with bpy.context.temp_override(**get_view3d_context()):
            bpy.context.scene.frame_set(-1)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")

        key_object = bpy.context.active_object
        fcu = _first_fcurve(key_object)
        for i in range(key_count):
            fcu.keyframe_points.insert(frame=i / key_count + frame_offset, value=0)

        with bpy.context.temp_override(**get_view3d_context()):
            for frame in range(key_count):
                bpy.context.scene.frame_set(frame_offset, subframe=frame / key_count)
                bpy.ops.anim.keyframe_delete_by_name(type="Location")

        # Only the key on frame -1 should be left
        # This works even though there are floating point precision issues,
        # because the deletion has the exact same precision as the insertion.
        # Due to that, the code calls keyframe_delete_by_name for
        # every floating point step multiple times.
        self.assertEqual(len(fcu.keyframe_points), 1)

        bpy.ops.object.delete(use_global=False)


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
