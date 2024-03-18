# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
blender -b -noaudio --factory-startup --python tests/python/bl_rigging_symmetrize.py -- --testdir /path/to/tests/data/animation
"""

import pathlib
import sys
import unittest

import bpy


def check_loc_rot_scale(self, bone, exp_bone):
    # Check if posistions are the same
    self.assertEqualVector(
        bone.head, exp_bone.head, "Head position", bone.name)
    self.assertEqualVector(
        bone.tail, exp_bone.tail, "Tail position", bone.name)

    # Scale
    self.assertEqualVector(
        bone.scale, exp_bone.scale, "Scale", bone.name)

    # Rotation
    rot_mode = exp_bone.rotation_mode
    self.assertEqual(bone.rotation_mode, rot_mode, "Rotations mode does not match on bone %s" % (bone.name))

    if rot_mode == 'QUATERNION':
        self.assertEqualVector(
            bone.rotation_quaternion, exp_bone.rotation_quaternion, "Quaternion rotation", bone.name)
    elif rot_mode == 'AXIS_ANGLE':
        self.assertEqualVector(
            bone.axis_angle, exp_bone.axis_angle, "Axis Angle rotation", bone.name)
    else:
        # Euler rotation
        self.assertEqualVector(
            bone.rotation_euler, exp_bone.rotation_euler, "Euler rotation", bone.name)


def check_parent(self, bone, exp_bone):
    self.assertEqual(type(bone.parent), type(exp_bone.parent),
                     "Mismatching types in pose.bones[%s].parent" % (bone.name))
    self.assertTrue(bone.parent is None or bone.parent.name == exp_bone.parent.name,
                    "Bone parent does not match on bone %s" % (bone.name))


def check_bendy_bones(self, bone, exp_bone):
    bone_variables = bone.bl_rna.properties.keys()

    bendy_bone_variables = [
        var for var in bone_variables if var.startswith("bbone_")]

    for var in bendy_bone_variables:
        value = getattr(bone, var)
        exp_value = getattr(exp_bone, var)

        self.assertEqual(type(value), type(exp_value),
                         "Mismatching types in pose.bones[%s].%s" % (bone.name, var))

        if isinstance(value, str):
            self.assertEqual(value, exp_value,
                             "Mismatching value in pose.bones[%s].%s" % (bone.name, var))
        elif hasattr(value, "name"):
            self.assertEqual(value.name, exp_value.name,
                             "Mismatching value in pose.bones[%s].%s" % (bone.name, var))
        else:
            self.assertAlmostEqual(value, exp_value,
                                   "Mismatching value in pose.bones[%s].%s" % (bone.name, var))


def check_ik(self, bone, exp_bone):
    bone_variables = bone.bl_rna.properties.keys()
    prefixes = ("ik_", "lock_ik", "use_ik")
    ik_bone_variables = (
        var for var in bone_variables
        if var.startswith(prefixes)
    )

    for var in ik_bone_variables:
        value = getattr(bone, var)
        exp_value = getattr(exp_bone, var)
        self.assertAlmostEqual(value, exp_value,
                               "Mismatching value in pose.bones[%s].%s" % (bone.name, var))


def check_constraints(self, input_arm, expected_arm, bone, exp_bone):
    const_len = len(bone.constraints)
    expo_const_len = len(exp_bone.constraints)

    self.assertEqual(const_len, expo_const_len,
                     "Constraints mismatch on bone %s" % (bone.name))

    for exp_constraint in exp_bone.constraints:
        const_name = exp_constraint.name
        # Make sure that the constraint exists
        self.assertTrue(const_name in bone.constraints,
                        "Bone %s is expected to contain constraint %s, but it does not." % (
                            bone.name, const_name))
        constraint = bone.constraints[const_name]
        const_variables = constraint.bl_rna.properties.keys()

        for var in const_variables:

            if var == "is_override_data":
                # This variable is not used for local (non linked) data.
                # For local object it is not initialized, so don't check this value.
                continue

            value = getattr(constraint, var)
            exp_value = getattr(exp_constraint, var)

            self.assertEqual(type(value), type(exp_value),
                             "Mismatching constraint value types in pose.bones[%s].constraints[%s].%s" % (
                             bone.name, const_name, var))

            if isinstance(value, str):
                self.assertEqual(value, exp_value,
                                 "Mismatching constraint value in pose.bones[%s].constraints[%s].%s" % (
                                     bone.name, const_name, var))
            elif hasattr(value, "name"):
                # Some constraints targets the armature itself, so the armature name should mismatch.
                if value.name == input_arm.name and exp_value.name == expected_arm.name:
                    continue

                self.assertEqual(value.name, exp_value.name,
                                 "Mismatching constraint value in pose.bones[%s].constraints[%s].%s" % (
                                     bone.name, const_name, var))

            elif isinstance(value, bool):
                self.assertEqual(value, exp_value,
                                 "Mismatching constraint boolean in pose.bones[%s].constraints[%s].%s" % (
                                     bone.name, const_name, var))
            else:
                msg = "Mismatching constraint value in pose.bones[%s].constraints[%s].%s" % (
                    bone.name, const_name, var)
                self.assertAlmostEqual(value, exp_value, places=6, msg=msg)


class AbstractAnimationTest:
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        'Test dir %s should exist' % self.testdir)


class ArmatureSymmetrizeTest(AbstractAnimationTest, unittest.TestCase):
    def test_symmetrize_operator(self):
        """Test that the symmetrize operator is working correctly."""
        bpy.ops.wm.open_mainfile(filepath=str(
            self.testdir / "symm_test.blend"))

        # #81541 (D9214)
        arm = bpy.data.objects['transform_const_rig']
        expected_arm = bpy.data.objects['expected_transform_const_rig']
        self.assertEqualSymmetrize(arm, expected_arm)

        # #66751 (D6009)
        arm = bpy.data.objects['dragon_rig']
        expected_arm = bpy.data.objects['expected_dragon_rig']
        self.assertEqualSymmetrize(arm, expected_arm)

    def assertEqualSymmetrize(self, input_arm, expected_arm):

        # Symmetrize our input armature
        bpy.context.view_layer.objects.active = input_arm
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.armature.select_all(action='SELECT')
        bpy.ops.armature.symmetrize()
        bpy.ops.object.mode_set(mode='OBJECT')

        # Make sure that the bone count is the same
        bone_len = len(input_arm.pose.bones)
        expected_bone_len = len(expected_arm.pose.bones)
        self.assertEqual(bone_len, expected_bone_len,
                         "Expected bone count to match")

        for exp_bone in expected_arm.pose.bones:
            bone_name = exp_bone.name
            # Make sure that the bone exists
            self.assertTrue(bone_name in input_arm.pose.bones,
                            "Armature is expected to contain bone %s, but it does not." % (bone_name))
            bone = input_arm.pose.bones[bone_name]

            # Loc Rot Scale
            check_loc_rot_scale(self, bone, exp_bone)

            # Parent settings
            check_parent(self, bone, exp_bone)

            # Bendy Bones
            check_bendy_bones(self, bone, exp_bone)

            # IK
            check_ik(self, bone, exp_bone)

            # Constraints
            check_constraints(self, input_arm, expected_arm, bone, exp_bone)

    def assertEqualVector(self, vec1, vec2, check_str, bone_name) -> None:
        for idx, value in enumerate(vec1):
            self.assertAlmostEqual(
                value, vec2[idx], 3, "%s does not match with expected value on bone %s" % (check_str, bone_name))


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
