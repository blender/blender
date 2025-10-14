# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
blender -b --factory-startup --python tests/python/bl_rigging_symmetrize.py -- --testdir /path/to/tests/files/animation
"""

import pathlib
import sys
import unittest

import bpy


def check_loc_rot_scale(self, bone, exp_bone):
    # Check if positions are the same
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

            if isinstance(value, bpy.types.bpy_prop_collection):
                # Don't compare collection properties.
                continue

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
            elif isinstance(value, float):
                msg = "Mismatching constraint value in pose.bones[%s].constraints[%s].%s" % (
                    bone.name, const_name, var)
                self.assertAlmostEqual(value, exp_value, places=6, msg=msg)
            elif isinstance(value, int):
                msg = "Mismatching constraint value in pose.bones[%s].constraints[%s].%s" % (
                    bone.name, const_name, var)
                self.assertEqual(value, exp_value, msg=msg)
            elif isinstance(value, bpy.types.ActionSlot):
                msg = "Mismatching constraint ActionSlot in pose.bones[%s].constraints[%s].%s" % (
                    bone.name, const_name, var)
                self.assertEqual(value, exp_value, msg=msg)
            elif value is None:
                # Since above the types were compared already, if value is none, so is exp_value.
                pass
            else:
                self.fail(f"unexpected value type: {value!r} is of type {type(value)}")


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


def create_armature() -> tuple[bpy.types.Object, bpy.types.Armature]:
    arm = bpy.data.armatures.new('Armature')
    arm_ob = bpy.data.objects.new('ArmObject', arm)

    # Link to the scene just for giggles. And ease of debugging when things
    # go bad.
    bpy.context.scene.collection.objects.link(arm_ob)

    return arm_ob, arm


def set_edit_bone_selected(ebone: bpy.types.EditBone, selected: bool):
    # Helper to select all parts of an edit bone.
    ebone.select = selected
    ebone.select_tail = selected
    ebone.select_head = selected


def create_copy_loc_constraint(pose_bone, target_ob, subtarget):
    pose_bone.constraints.new("COPY_LOCATION")
    pose_bone.constraints[0].target = target_ob
    pose_bone.constraints[0].subtarget = subtarget


class ArmatureSymmetrizeTargetsTest(unittest.TestCase):
    arm_ob: bpy.types.Object
    arm: bpy.types.Armature

    def setUp(self):
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        self.arm_ob, self.arm = create_armature()
        bpy.context.view_layer.objects.active = self.arm_ob
        bpy.ops.object.mode_set(mode='EDIT')
        ebone = self.arm.edit_bones.new(name="test.l")
        ebone.tail = (1, 0, 0)

    def test_symmetrize_selection(self):
        # Only selected things are symmetrized.
        set_edit_bone_selected(self.arm.edit_bones["test.l"], False)
        bpy.ops.armature.symmetrize()
        self.assertEqual(len(self.arm.edit_bones), 1, "If nothing is selected, no bone is symmetrized")

        set_edit_bone_selected(self.arm.edit_bones["test.l"], True)
        bpy.ops.armature.symmetrize()
        self.assertEqual(len(self.arm.edit_bones), 2, "Selected EditBone should have been symmetrized")
        self.assertTrue("test.r" in self.arm.edit_bones)
        self.assertTrue("test.l" in self.arm.edit_bones)

    def test_symmetrize_constraint_sub_target(self):
        # Explicitly test that constraints targeting another armature are symmetrized.
        bpy.ops.object.mode_set(mode='OBJECT')
        target_arm_ob, target_arm = create_armature()
        bpy.context.view_layer.objects.active = target_arm_ob
        bpy.ops.object.mode_set(mode='EDIT')
        target_arm.edit_bones.new("target.l")
        target_arm.edit_bones.new("target.r")
        target_arm.edit_bones["target.l"].tail = (1, 0, 0)
        target_arm.edit_bones["target.r"].tail = (1, 0, 0)

        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.context.view_layer.objects.active = self.arm_ob

        bpy.ops.object.mode_set(mode='POSE')
        pose_bone_l = self.arm_ob.pose.bones["test.l"]
        create_copy_loc_constraint(pose_bone_l, target_arm_ob, "target.l")

        bpy.ops.object.mode_set(mode='EDIT')
        set_edit_bone_selected(self.arm.edit_bones["test.l"], True)
        bpy.ops.armature.symmetrize()
        self.assertEqual(len(self.arm.edit_bones), 2, "Bone should have been symmetrized")
        self.assertTrue("test.r" in self.arm.edit_bones)
        self.assertTrue("test.l" in self.arm.edit_bones)

        bpy.ops.object.mode_set(mode='POSE')
        self.assertEqual(len(self.arm_ob.pose.bones["test.r"].constraints), 1, "Constraint should have been copied")
        symm_constraint = self.arm_ob.pose.bones["test.r"].constraints[0]
        self.assertEqual(symm_constraint.subtarget, "target.r")

    def test_symmetrize_invalid_subtarget(self):
        # Blender shouldn't crash when there is an invalid subtarget specified.
        bpy.ops.object.mode_set(mode='OBJECT')
        target_ob = bpy.data.objects.new("target", None)
        bpy.context.scene.collection.objects.link(target_ob)

        bpy.context.view_layer.objects.active = self.arm_ob

        bpy.ops.object.mode_set(mode='POSE')
        pose_bone_l = self.arm_ob.pose.bones["test.l"]
        create_copy_loc_constraint(pose_bone_l, target_ob, "invalid_subtarget")

        bpy.ops.object.mode_set(mode='EDIT')
        set_edit_bone_selected(self.arm.edit_bones["test.l"], True)
        bpy.ops.armature.symmetrize()
        self.assertEqual(len(self.arm.edit_bones), 2, "Bone should have been symmetrized")
        self.assertTrue("test.r" in self.arm.edit_bones)
        self.assertTrue("test.l" in self.arm.edit_bones)

        bpy.ops.object.mode_set(mode='POSE')
        self.assertEqual(len(self.arm_ob.pose.bones["test.r"].constraints), 1, "Constraint should have been copied")
        symm_constraint = self.arm_ob.pose.bones["test.r"].constraints[0]
        self.assertEqual(symm_constraint.subtarget, "invalid_subtarget")


class ArmatureSymmetrizeCollectionAssignments(unittest.TestCase):
    arm_ob: bpy.types.Object
    arm: bpy.types.Armature

    def setUp(self):
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        self.arm_ob, self.arm = create_armature()
        bpy.context.view_layer.objects.active = self.arm_ob
        bpy.ops.object.mode_set(mode='EDIT')
        ebone = self.arm.edit_bones.new(name="test.l")
        ebone.tail = (1, 0, 0)

        parent_coll = self.arm.collections.new("parent")
        left_coll = self.arm.collections.new("collection.l", parent=parent_coll)
        self.assertTrue(left_coll.assign(ebone))
        self.assertEqual(len(ebone.collections), 1)

    def test_symmetrize_to_existing_collection(self):
        other_parent = self.arm.collections.new("other_parent")
        right_coll = self.arm.collections.new("collection.r", parent=other_parent)

        set_edit_bone_selected(self.arm.edit_bones["test.l"], True)
        bpy.ops.armature.symmetrize()

        right_bone = self.arm.edit_bones["test.r"]
        self.assertEqual(len(right_bone.collections), 1)
        self.assertEqual(right_bone.collections[0], right_coll)

        # Parents should not be modified.
        left_coll = self.arm.collections_all["collection.l"]
        self.assertNotEqual(right_coll.parent, left_coll.parent)

    def test_no_symmetrize(self):
        # If the collection name cannot be flipped, nothing changes.
        non_flip_collection = self.arm.collections.new("foobar")
        left_bone = self.arm.edit_bones["test.l"]
        self.arm.collections_all["collection.l"].unassign(left_bone)
        self.assertTrue(non_flip_collection.assign(left_bone))

        set_edit_bone_selected(left_bone, True)
        bpy.ops.armature.symmetrize()

        right_bone = self.arm.edit_bones["test.r"]
        self.assertEqual(len(right_bone.collections), 1)
        self.assertEqual(right_bone.collections[0], non_flip_collection)

    def test_create_missing_collection(self):
        set_edit_bone_selected(self.arm.edit_bones["test.l"], True)
        self.assertFalse("collection.r" in self.arm.collections_all)
        bpy.ops.armature.symmetrize()

        # Missing collections are created.
        self.assertTrue("collection.r" in self.arm.collections_all)
        right_coll = self.arm.collections_all["collection.r"]
        # When the collection is created, it is parented to the same collection as the source collection.
        left_coll = self.arm.collections_all["collection.l"]
        self.assertEqual(right_coll.parent, left_coll.parent)

        right_bone = self.arm.edit_bones["test.r"]
        self.assertEqual(len(right_bone.collections), 1)
        self.assertEqual(right_bone.collections[0], right_coll)

    def test_symmetrize_to_existing_bone(self):
        right_bone = self.arm.edit_bones.new(name="test.r")
        right_bone.tail = (1, 0, 0)
        unique_right_coll = self.arm.collections.new("unique")
        unique_right_coll.assign(right_bone)

        set_edit_bone_selected(self.arm.edit_bones["test.l"], True)
        bpy.ops.armature.symmetrize()

        # Missing collection is created.
        self.assertTrue("collection.r" in self.arm.collections_all)
        self.assertEqual(len(right_bone.collections), 2)
        self.assertTrue("collection.r" in right_bone.collections)
        self.assertTrue("unique" in right_bone.collections,
                        "Mirrored bone shouldn't have lost the unique collection assignment")

        # Symmetrizing twice shouldn't double invert the collection assignments.
        set_edit_bone_selected(self.arm.edit_bones["test.l"], True)
        set_edit_bone_selected(right_bone, False)
        bpy.ops.armature.symmetrize()
        self.assertTrue("collection.r" in right_bone.collections)
        self.assertTrue("collection.l" not in right_bone.collections)


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
