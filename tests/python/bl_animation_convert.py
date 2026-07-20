# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import math
import sys
import unittest
import pathlib
from typing import Generator

import bpy
import mathutils


"""
blender -b --factory-startup --python tests/python/bl_animation_convert.py -- --testdir tests/files/animation/
"""


def _fcurves_with_rna_path(action: bpy.types.Action, slot: bpy.types.ActionSlot,
                           rna_path: str) -> Generator[bpy.types.FCurve]:
    for layer in action.layers:
        for strip in layer.strips:
            channelbag = strip.channelbag(slot)
            if not channelbag:
                continue
            for fcurve in channelbag.fcurves:
                if fcurve.data_path == rna_path:
                    yield fcurve


def _action_slot_has_rna_path(action: bpy.types.Action, slot: bpy.types.ActionSlot, rna_path: str) -> bool:
    return any(_fcurves_with_rna_path(action, slot, rna_path))


class ConvertRotationModeBase(unittest.TestCase):
    def _assert_almost_equal_rotation_matrix(self, a: mathutils.Matrix, b: mathutils.Matrix):
        for j in range(3):
            for i in range(3):
                if abs(a.row[i][j] - b.row[i][j]) > 0.001:
                    raise AssertionError(f"Rotation part of matrices doesn't match\n{a}\n{b}")

    def _assert_almost_equal_euler(self, a: mathutils.Euler, b: mathutils.Euler):
        msg = f"Difference in Euler: {a} - {b}"
        self.assertAlmostEqual(a.x, b.x, 2, msg)
        self.assertAlmostEqual(a.y, b.y, 2, msg)
        self.assertAlmostEqual(a.z, b.z, 2, msg)

    def _assert_almost_equal_quat(self, a: mathutils.Quaternion, b: mathutils.Quaternion):
        msg = f"Difference in Quaternion: {a} - {b}"
        self.assertAlmostEqual(a.x, b.x, 2, msg)
        self.assertAlmostEqual(a.y, b.y, 2, msg)
        self.assertAlmostEqual(a.z, b.z, 2, msg)
        self.assertAlmostEqual(a.w, b.w, 2, msg)


class ConvertRotationModeObject(ConvertRotationModeBase):
    action: bpy.types.Action
    action_slot: bpy.types.ActionSlot
    keyed_frames = [1, 6, 11, 16, 21]

    obj: bpy.types.Object
    reference_obj: bpy.types.Object

    def setUp(self) -> None:
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "rotation_mode_conversion.blend"))
        self.obj = bpy.data.objects["Suzanne"]
        # The NLA object has the exact same animation, so it is possible to compare rotation matrices.
        self.reference_obj = bpy.data.objects["Suzanne_NLA"]
        self.action = self.obj.animation_data.action
        self.action_slot = self.obj.animation_data.action_slot
        self.assertEqual(self.obj.rotation_mode, 'XYZ')

    def test_convert_to_quaternion(self):
        self.obj.convert_rotation_mode('QUATERNION')
        self.assertEqual(self.obj.rotation_mode, 'QUATERNION')
        for frame in self.keyed_frames:
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_rotation_matrix(self.reference_obj.matrix_world, self.obj.matrix_world)

    def test_convert_to_zxy(self):
        self.obj.convert_rotation_mode('ZXY')
        for frame in self.keyed_frames:
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_rotation_matrix(self.reference_obj.matrix_world, self.obj.matrix_world)

    def test_convert_rotation_bake(self):
        self.obj.convert_rotation_mode('QUATERNION', bake=True)

        fcurves = _fcurves_with_rna_path(
            self.action,
            self.action_slot,
            'rotation_quaternion')

        for fcurve in fcurves:
            self.assertEqual(len(fcurve.keyframe_points), 21)

        for frame in range(self.keyed_frames[0], self.keyed_frames[1] + 1):
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_rotation_matrix(self.reference_obj.matrix_world, self.obj.matrix_world)


class ConvertRotationModeNLA(ConvertRotationModeBase):
    """Verifying that actions stored in the NLA of an object are also converted."""
    action: bpy.types.Action
    action_slot: bpy.types.ActionSlot
    keyed_frames = [1, 6, 11, 16, 21]

    nla_object: bpy.types.Object
    reference_object: bpy.types.Object

    def setUp(self) -> None:
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "rotation_mode_conversion.blend"))
        self.nla_object = bpy.data.objects["Suzanne_NLA"]
        self.reference_object = bpy.data.objects["Suzanne"]

    def test_convert_to_quaternion(self):
        self.nla_object.convert_rotation_mode('QUATERNION')
        for frame in self.keyed_frames:
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_rotation_matrix(self.nla_object.matrix_world, self.reference_object.matrix_world)

    def test_convert_to_zxy(self):
        self.nla_object.convert_rotation_mode('ZXY')
        for frame in self.keyed_frames:
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_rotation_matrix(self.nla_object.matrix_world, self.reference_object.matrix_world)


class ConvertRotationModeBones(ConvertRotationModeBase):

    action: bpy.types.Action
    action_slot: bpy.types.ActionSlot
    keyed_frames = [1, 6, 11, 16, 21]

    bone_quat: bpy.types.PoseBone
    bone_axis_angle: bpy.types.PoseBone
    bone_xyz: bpy.types.PoseBone
    bone_zyx: bpy.types.PoseBone

    bone_euler_360: bpy.types.PoseBone
    bone_quat_to_xyz_bake: bpy.types.PoseBone

    bone_no_rotation_keys: bpy.types.PoseBone
    bone_partially_keyed: bpy.types.PoseBone
    bone_subframes: bpy.types.PoseBone
    bone_keyed_rotation_mode: bpy.types.PoseBone

    def setUp(self) -> None:
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "rotation_mode_conversion.blend"))
        armature_ob = bpy.data.objects["Armature"]
        self.action = armature_ob.animation_data.action
        self.action_slot = armature_ob.animation_data.action_slot
        pose = armature_ob.pose

        self.bone_quat = pose.bones["bone_quat"]
        self.bone_axis_angle = pose.bones["bone_axis_angle"]
        self.bone_xyz = pose.bones["bone_xyz"]
        self.bone_zyx = pose.bones["bone_zyx"]

        self.bone_euler_360 = pose.bones["bone_euler_rotation_360"]
        self.bone_quat_to_xyz_bake = pose.bones["bone_quat_to_xyz_bake"]

        self.bone_no_rotation_keys = pose.bones["bone_no_rotation_keys"]
        self.bone_partially_keyed = pose.bones["bone_partially_keyed"]
        self.bone_subframes = pose.bones["bone_subframe_keys"]
        self.bone_keyed_rotation_mode = pose.bones["bone_keyed_rotation_mode"]

    def test_convert_quat_to_xyz(self):
        self.bone_quat.convert_rotation_mode('XYZ')
        self.assertEqual(self.bone_quat.rotation_mode, 'XYZ')
        for frame in self.keyed_frames:
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_euler(self.bone_quat.rotation_euler, self.bone_xyz.rotation_euler)

    def test_convert_xyz_to_zyx(self):
        self.bone_xyz.convert_rotation_mode('ZYX')
        self.assertEqual(self.bone_xyz.rotation_mode, 'ZYX')
        for frame in self.keyed_frames:
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_euler(self.bone_xyz.rotation_euler, self.bone_zyx.rotation_euler)

    def test_convert_axis_angle_to_quat(self):
        self.bone_axis_angle.convert_rotation_mode('QUATERNION')
        self.assertEqual(self.bone_axis_angle.rotation_mode, 'QUATERNION')
        for frame in self.keyed_frames:
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_quat(self.bone_axis_angle.rotation_quaternion, self.bone_quat.rotation_quaternion)

    def test_convert_xyz_to_quat(self):
        self.bone_xyz.convert_rotation_mode('QUATERNION')
        self.assertEqual(self.bone_xyz.rotation_mode, 'QUATERNION')
        for frame in self.keyed_frames:
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_quat(self.bone_xyz.rotation_quaternion, self.bone_quat.rotation_quaternion)

    def test_convert_360_rotation_limitation(self):
        """ When converting from euler with deltas >180 degrees between keyframes,
        the resulting animation loses that information."""
        bpy.context.scene.frame_set(21)
        self.assertAlmostEqual(self.bone_euler_360.rotation_euler.x, math.radians(360), 2)
        self.assertEqual(self.bone_euler_360.rotation_mode, 'XYZ')
        self.bone_euler_360.convert_rotation_mode('XYZ')
        # Trying to convert to the current rotation mode should be a no op.
        self.assertAlmostEqual(self.bone_euler_360.rotation_euler.x, math.radians(360), 2)

        self.bone_euler_360.convert_rotation_mode('XZY')
        # Ensure depsgraph is evaluated so animation data is refreshed.
        bpy.context.evaluated_depsgraph_get()
        # The information about the 360 degree rotation is lost in this case.
        self.assertAlmostEqual(self.bone_euler_360.rotation_euler.x, math.radians(0), 2)

    def test_convert_360_rotation_bake(self):
        """ When converting rotations >180 degrees baking has to
        be used to ensure the interpolation is preserved. """
        fcurves = _fcurves_with_rna_path(
            self.action,
            self.action_slot,
            'pose.bones["bone_euler_rotation_360"].rotation_euler')

        for fcurve in fcurves:
            self.assertEqual(len(fcurve.keyframe_points), 2)
            self.assertAlmostEqual(fcurve.keyframe_points[1].co[0], 21, 2)

        self.bone_euler_360.convert_rotation_mode('QUATERNION', bake=True)

        fcurves = _fcurves_with_rna_path(
            self.action,
            self.action_slot,
            'pose.bones["bone_euler_rotation_360"].rotation_quaternion')

        for fcurve in fcurves:
            self.assertEqual(len(fcurve.keyframe_points), 21)

        bpy.context.scene.frame_set(1)
        # Converting to a list so the value is copied. Otherwise it will be
        # modified while stepping through the timeline.
        prev_quat = list(self.bone_euler_360.rotation_quaternion)
        for i in range(2, 21):
            bpy.context.scene.frame_set(i)
            quat = list(self.bone_euler_360.rotation_quaternion)
            self.assertNotEqual(quat, prev_quat, f"Identical Quaternions on frame {i}")
            prev_quat = quat

    def test_bake_conversion(self):
        """When baking, all animation on full frames should be preserved. Subframe interpolation may deviate."""
        # bone_quat_to_xyz_bake and bone_quat have identical animation. By
        # converting one of them to xyz we can confirm that the baking preserves
        # interpolation. We cannot compare a baked quaternion to the bone_xyz
        # since euler and quaternion interpolate differently.
        self.bone_quat_to_xyz_bake.convert_rotation_mode('XYZ', bake=True)
        for i in range(self.keyed_frames[0], self.keyed_frames[1] + 1):
            bpy.context.scene.frame_set(i)
            # We have to compare to the quat bone here,
            self._assert_almost_equal_rotation_matrix(self.bone_quat_to_xyz_bake.matrix, self.bone_quat.matrix)

    def test_convert_keyed_rotation_mode(self):
        """ When the rotation mode itself is keyed and changes during the animation,
        the conversion has to make sure the animation is preserved. """
        keyed_frames = [1, 6, 7, 11, 12, 21]
        matrices_before_conversion: list[mathutils.Matrix] = []
        for frame in keyed_frames:
            bpy.context.scene.frame_set(frame)
            matrices_before_conversion.append(self.bone_keyed_rotation_mode.matrix.copy())
        self.bone_keyed_rotation_mode.convert_rotation_mode('QUATERNION')

        for i, frame in enumerate(keyed_frames):
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_rotation_matrix(
                matrices_before_conversion[i],
                self.bone_keyed_rotation_mode.matrix)
            self.assertEqual(self.bone_keyed_rotation_mode.rotation_mode, 'QUATERNION')

    def test_convert_unkeyed_rotation(self):
        """ When converting the rotation mode on a bone that has no keys on its rotation channels,
         the function should just change the rotation mode while preserving the visual rotation. """
        matrix_before: mathutils.Matrix = self.bone_no_rotation_keys.matrix.copy()
        self.assertEqual(self.bone_no_rotation_keys.rotation_mode, 'QUATERNION')
        # No animation for the rotation mode.
        self.assertFalse(
            _action_slot_has_rna_path(
                self.action,
                self.action_slot,
                'pose.bones["bone_no_rotation_keys"].rotation_quaternion'))
        self.bone_no_rotation_keys.convert_rotation_mode('XYZ')
        bpy.context.evaluated_depsgraph_get()
        matrix_after: mathutils.Matrix = self.bone_no_rotation_keys.matrix
        self._assert_almost_equal_rotation_matrix(matrix_before, matrix_after)
        self.assertFalse(
            _action_slot_has_rna_path(
                self.action,
                self.action_slot,
                'pose.bones["bone_no_rotation_keys"].rotation_euler'))

    def test_result_is_euler_filtered(self):
        """ When converting rotation modes we should not have sudden 180 degree jumps in euler mode. """
        self.bone_euler_360.convert_rotation_mode('XZY', bake=True)
        fcurves = _fcurves_with_rna_path(
            self.action,
            self.action_slot,
            'pose.bones["bone_euler_rotation_360"].rotation_euler')
        for fcurve in fcurves:
            if fcurve.array_index != 0:
                # Only expecting rotation on x.
                continue
            prev_value = fcurve.evaluate(frame=1)
            for i in range(2, 21):
                value = fcurve.evaluate(i)
                # Whatever rotation, the code should choose the one closest to the previous rotation.
                self.assertLess(abs(prev_value - value), 2 * math.pi)
                prev_value = value

    def test_convert_partially_keyed_rotation(self):
        """ When converting rotations without baking the resulting animation will have all channels keyed if at least one channel has a key on a frame. """
        fcurves = _fcurves_with_rna_path(
            self.action,
            self.action_slot,
            'pose.bones["bone_partially_keyed"].rotation_euler')
        self.assertEqual(len(list(fcurves)), 2)
        self.bone_partially_keyed.convert_rotation_mode('XZY')

        fcurves = _fcurves_with_rna_path(
            self.action,
            self.action_slot,
            'pose.bones["bone_partially_keyed"].rotation_euler')
        self.assertEqual(len(list(fcurves)), 3)
        expected_frames = [1, 6, 16, 21]
        for fcurve in fcurves:
            for i, frame in enumerate(expected_frames):
                self.assertAlmostEqual(fcurve.keyframe_points[i].co[0], frame, 2)

    def test_convert_subframes(self):
        self.assertEqual(self.bone_subframes.rotation_mode, 'XYZ')
        fcurves = _fcurves_with_rna_path(
            self.action,
            self.action_slot,
            'pose.bones["bone_subframe_keys"].rotation_euler')
        self.assertEqual(len(list(fcurves)), 3)
        self.bone_subframes.convert_rotation_mode('XZY')
        self.assertEqual(self.bone_subframes.rotation_mode, 'XZY')
        # Have to get the new FCurves since the call will always replace the FCurves even if conversion Euler to Euler.
        fcurves = _fcurves_with_rna_path(
            self.action,
            self.action_slot,
            'pose.bones["bone_subframe_keys"].rotation_euler')
        expected_frames = [1, 2.5, 7.2]
        for fcurve in fcurves:
            for i, frame in enumerate(expected_frames):
                self.assertAlmostEqual(fcurve.keyframe_points[i].co[0], frame, 2)


def main():
    global args
    import argparse

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)

    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
