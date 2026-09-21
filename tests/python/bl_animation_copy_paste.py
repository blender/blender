# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import bpy
import pathlib
import tempfile
import mathutils

import unittest
import sys


TEST_FILE = "world_space_copy_paste.blend"
COPYBUFFER_NAME = "world_space_buffer.blend"


def _set_select_all_bones(armature_ob: bpy.types.Object, select: bool) -> None:
    for bone in armature_ob.pose.bones:
        bone.select = select


class AbstractCopyPasteTest(unittest.TestCase):

    def setUp(self) -> None:
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / TEST_FILE))
        for obj in bpy.data.objects:
            obj.select_set(False)
        # The copybuffer is stored in whatever is returned from `BKE_tempdir_base`.
        # By ensuring that the user pref is empty the used path should always be the
        # system temporary directory.
        bpy.context.preferences.filepaths.temporary_directory = ""
        self._copybuffer_path = pathlib.Path(tempfile.gettempdir()) / COPYBUFFER_NAME

    def _assert_almost_equal_matrix(self, a: mathutils.Matrix, b: mathutils.Matrix):
        equal = True
        for j in range(4):
            for i in range(4):
                if abs(a.row[i][j] - b.row[i][j]) > 0.001:
                    equal = False
                    break
        if not equal:
            raise self.failureException(f"Matrices don't match\n{a}\n{b}")

    def _assert_almost_equal_lists(self, a: list[float], b: list[float]):
        equal = True
        assert len(a) == len(b)
        for i in range(len(a)):
            if abs(a[i] - b[i]) > 0.001:
                equal = False
                break
        if not equal:
            raise self.failureException(f"Lists don't match\n{a}\n{b}")

    def _assert_bones_equal_world_space(
            self,
            arm_a: bpy.types.Object,
            bone_a: bpy.types.PoseBone,
            arm_b: bpy.types.Object,
            bone_b: bpy.types.PoseBone) -> None:
        for frame in range(10):
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_matrix(arm_a.matrix_world @ bone_a.matrix, arm_b.matrix_world @ bone_b.matrix)

    def _assert_objects_equal_world_space(self, obj_a: bpy.types.Object, obj_b: bpy.types.Object) -> None:
        for frame in range(10):
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_matrix(obj_a.matrix_world, obj_b.matrix_world)


class WorldSpaceCopyTest(AbstractCopyPasteTest):
    """
    Test that copying creates the expected temp file and confirming
    that temp file contains the correct data.
    """

    def tearDown(self) -> None:
        if self._copybuffer_path.exists():
            os.remove(self._copybuffer_path)

    def test_invalid_range(self) -> None:
        obj = bpy.data.objects["armature_simple"]
        obj.select_set(True)
        # Passing an invalid frame range will error.
        with self.assertRaises(RuntimeError):
            bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=10, end=0)
        with self.assertRaises(RuntimeError):
            bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=2, end=1)
        self.assertFalse(self._copybuffer_path.exists())

    def _test_for_single_entity_in_buffer(self, entity_name):
        # Asserts that there are only FCurves with matching rna paths in the buffer.
        bpy.ops.wm.open_mainfile(filepath=self._copybuffer_path.as_posix())
        self.assertEqual(len(bpy.data.actions), 1)
        buffer_action = bpy.data.actions[0]
        buffer_fcurves = buffer_action.layers[0].strips[0].channelbags[0].fcurves
        # A matrix is stored in 12 FCurves. The last row of the 4x4 matrix is assumed
        # to always be 0/0/0/1.
        self.assertEqual(len(buffer_fcurves), 12)
        for fcurve in buffer_fcurves:
            self.assertEqual(fcurve.data_path, entity_name)
            # Data is not stored in BezTriple keyframes.
            self.assertEqual(len(fcurve.keyframe_points), 0)
            self.assertEqual(len(fcurve.sampled_points), 11)
            self.assertEqual(fcurve.sampled_points[0].co.x, 0)
            # Range is inclusive at the end so 10 is the last frame to be copied.
            self.assertEqual(fcurve.sampled_points[-1].co.x, 10)

    def test_copy_object(self) -> None:
        obj = bpy.data.objects["armature_simple"]
        obj_name = obj.name
        obj.select_set(True)
        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)
        self.assertTrue(self._copybuffer_path.exists())

        self._test_for_single_entity_in_buffer(obj_name)

    def test_copy_pose_bone(self) -> None:
        obj: bpy.types.Object = bpy.data.objects["armature_simple"]
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.mode_set(mode='POSE')
        pose_bone: bpy.types.PoseBone = obj.pose.bones[0]
        pose_bone.select = True
        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)
        self.assertTrue(self._copybuffer_path.exists())

        self._test_for_single_entity_in_buffer(pose_bone.name)

    def test_copy_object_no_anim(self) -> None:
        """The copying is done even for objects that are not animated."""
        obj: bpy.types.Object = bpy.data.objects["armature_no_anim"]
        obj.select_set(True)
        obj_name = obj.name
        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)
        self.assertTrue(self._copybuffer_path.exists())
        bpy.ops.wm.open_mainfile(filepath=self._copybuffer_path.as_posix())
        buffer_action = bpy.data.actions[0]
        buffer_fcurves = buffer_action.layers[0].strips[0].channelbags[0].fcurves
        for fcurve in buffer_fcurves:
            self.assertEqual(fcurve.data_path, obj_name)
            self.assertEqual(len(fcurve.sampled_points), 11)
            # All the FCurves will be flat.
            self.assertEqual(fcurve.sampled_points[0].co.y, fcurve.sampled_points[-1].co.y)


class WorldSpacePasteTest(AbstractCopyPasteTest):

    def test_paste_to_different_object(self) -> None:
        """Tests that pasting to a differently named object works in the simple 1:1 case."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_simple"]
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_single_bone"]
        copy_obj.select_set(True)
        paste_obj.select_set(False)

        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)

        copy_obj.select_set(False)
        paste_obj.select_set(True)

        bpy.ops.anim.world_space_paste()

        self._assert_objects_equal_world_space(copy_obj, paste_obj)

    def test_paste_static_animation(self) -> None:
        """Pasting static animation onto an entity that is not yet animated should not create new FCurves."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_no_anim"]
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_single_bone"]
        copy_obj.select_set(True)
        paste_obj.select_set(False)

        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)

        copy_obj.select_set(False)
        paste_obj.select_set(True)

        bpy.ops.anim.world_space_paste()

        paste_anim_data: bpy.types.AnimData = paste_obj.animation_data
        assert paste_anim_data is not None
        action: bpy.types.Action = paste_anim_data.action
        # The action is created.
        assert action is not None
        channelbag = action.layers[0].strips[0].channelbags[0]
        self.assertEqual(len(channelbag.fcurves), 0)

    def test_paste_pose_bone(self) -> None:
        """Tests that pasting to equally named bones in different armatures works."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_simple"]
        copy_obj.select_set(True)
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_single_bone"]
        paste_obj.select_set(True)

        bpy.ops.object.mode_set(mode='POSE')
        copy_bone: bpy.types.PoseBone = copy_obj.pose.bones[0]
        paste_bone: bpy.types.PoseBone = paste_obj.pose.bones[0]
        copy_bone.select = True
        paste_bone.select = False
        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)

        copy_bone.select = False
        paste_bone.select = True
        bpy.ops.anim.world_space_paste()

        self._assert_bones_equal_world_space(copy_obj, copy_bone, paste_obj, paste_bone)

    def test_paste_scale_animation(self) -> None:
        """Pasting an animation of non uniform scale values should work."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_scale_anim"]
        copy_obj.select_set(True)
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_single_bone"]
        paste_obj.select_set(True)

        bpy.ops.object.mode_set(mode='POSE')
        copy_bone: bpy.types.PoseBone = copy_obj.pose.bones[0]
        paste_bone: bpy.types.PoseBone = paste_obj.pose.bones[0]
        copy_bone.select = True
        paste_bone.select = False
        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)

        copy_bone.select = False
        paste_bone.select = True
        bpy.ops.anim.world_space_paste()

        self._assert_bones_equal_world_space(copy_obj, copy_bone, paste_obj, paste_bone)

    def test_paste_to_skewed_space(self) -> None:
        """Pasting into a skewed space will only correctly work for location.
        The world matrix will not match since it will contain a skew that we cannot correct for since
        bones store their transform data in separate loc/rot/scale values."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_simple"]
        bpy.context.view_layer.objects.active = copy_obj
        copy_obj.select_set(True)
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_skewed_space"]
        paste_obj.select_set(True)

        bpy.ops.object.mode_set(mode='POSE')
        copy_bone: bpy.types.PoseBone = copy_obj.pose.bones[0]
        paste_bone: bpy.types.PoseBone = paste_obj.pose.bones[2]
        # Deselect all bones to ensure we copy the right data.
        _set_select_all_bones(paste_obj, False)
        copy_bone.select = True
        paste_bone.select = False
        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)

        copy_bone.select = False
        paste_bone.select = True
        bpy.ops.anim.world_space_paste()

        for frame in range(10):
            bpy.context.scene.frame_set(frame)
            copy_matrix: mathutils.Matrix = copy_obj.matrix_world @ copy_bone.matrix
            paste_matrix: mathutils.Matrix = paste_obj.matrix_world @ paste_bone.matrix
            self._assert_almost_equal_lists(copy_matrix.to_translation(), paste_matrix.to_translation())
            # Rotation and scale will not match since they contain the skew.

    def test_indirect_animation(self) -> None:
        """The entity from which we copy may not be animated directly,
        copying the world space movement should still work."""
        copy_obj: bpy.types.Object = bpy.data.objects["indirect_motion_child"]
        copy_obj.select_set(True)
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_single_bone"]
        paste_obj.select_set(False)
        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)

        copy_obj.select_set(False)
        paste_obj.select_set(True)
        bpy.ops.anim.world_space_paste()

        self._assert_objects_equal_world_space(copy_obj, paste_obj)

    def test_from_objects_to_bones(self) -> None:
        """Copying between objects and bones works as long it is either 1:1 or the names match."""
        copy_obj: bpy.types.Object = bpy.data.objects["indirect_motion_parent"]
        copy_obj.select_set(True)
        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)
        copy_obj.select_set(False)

        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_single_bone"]
        bpy.context.view_layer.objects.active = paste_obj
        paste_obj.select_set(True)
        bpy.ops.object.mode_set(mode='POSE')
        paste_bone: bpy.types.PoseBone = paste_obj.pose.bones[0]
        paste_bone.select = True

        bpy.ops.anim.world_space_paste()

        for frame in range(10):
            bpy.context.scene.frame_set(frame)
            self._assert_almost_equal_matrix(copy_obj.matrix_world, paste_obj.matrix_world @ paste_bone.matrix)

    def test_pasting_to_connected_child(self) -> None:
        """When pasting to a bone that is connected, the location cannot be modified.
        As such, the location will not match when pasting to it. Rotation should still match though."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_simple"]
        copy_obj.select_set(True)
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_connected_child"]
        paste_obj.select_set(True)

        bpy.ops.object.mode_set(mode='POSE')
        copy_bone: bpy.types.PoseBone = copy_obj.pose.bones[0]
        paste_bone: bpy.types.PoseBone = paste_obj.pose.bones["child"]

        copy_bone.select = True
        paste_bone.select = False
        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)

        copy_bone.select = False
        paste_bone.select = True
        bpy.ops.anim.world_space_paste()

        for frame in range(10):
            bpy.context.scene.frame_set(frame)
            copy_matrix: mathutils.Matrix = copy_obj.matrix_world @ copy_bone.matrix
            paste_matrix: mathutils.Matrix = paste_obj.matrix_world @ paste_bone.matrix
            self._assert_almost_equal_lists(copy_matrix.to_quaternion(), paste_matrix.to_quaternion())
            self.assertNotEqual(copy_matrix.to_translation(), paste_matrix.to_translation())

    def test_paste_to_constrained_bone(self) -> None:
        """Depending on the constraint, the pasted bone may not be able to reach the required world space.
        There is also the case where the transformation is additive, like the armature constraint. In
        those cases, the pasting will not result in a correct transform."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_simple"]
        copy_obj.select_set(True)
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_constrained_bone"]
        paste_obj.select_set(True)

        bpy.ops.object.mode_set(mode='POSE')
        copy_bone: bpy.types.PoseBone = copy_obj.pose.bones[0]
        copy_bone.select = True
        _set_select_all_bones(paste_obj, False)
        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)
        copy_bone.select = False

        # Copy Location Constraint
        paste_bone_copy_loc: bpy.types.PoseBone = paste_obj.pose.bones["bone_copy_loc"]
        paste_bone_limit_loc: bpy.types.PoseBone = paste_obj.pose.bones["bone_limit_loc"]
        paste_bone_damped_track: bpy.types.PoseBone = paste_obj.pose.bones["bone_damped_track"]
        paste_bone_child_of: bpy.types.PoseBone = paste_obj.pose.bones["bone_child_of"]
        paste_bone_copy_loc.select = True
        paste_bone_limit_loc.select = True
        paste_bone_damped_track.select = True
        paste_bone_child_of.select = True
        # Pasting 1:n.
        bpy.ops.anim.world_space_paste()

        for frame in range(10):
            bpy.context.scene.frame_set(frame)
            copy_matrix: mathutils.Matrix = copy_obj.matrix_world @ copy_bone.matrix

            # The copy location constraint overrides the location so this won't match.
            paste_matrix_copy_loc: mathutils.Matrix = paste_obj.matrix_world @ paste_bone_copy_loc.matrix
            self.assertNotEqual(copy_matrix.to_translation(), paste_matrix_copy_loc.to_translation())
            self._assert_almost_equal_lists(copy_matrix.to_quaternion(), paste_matrix_copy_loc.to_quaternion())

            # The location is constrained to be 0 on Y and Z so only the X axis will match.
            paste_matrix_limit_loc: mathutils.Matrix = paste_obj.matrix_world @ paste_bone_limit_loc.matrix
            self.assertNotEqual(copy_matrix.to_translation(), paste_matrix_limit_loc.to_translation())
            self.assertAlmostEqual(copy_matrix.to_translation().x, paste_matrix_limit_loc.to_translation().x, 3)
            self._assert_almost_equal_lists(copy_matrix.to_quaternion(), paste_matrix_limit_loc.to_quaternion())

            # The damped track constraint overrides the rotation, so only location will match.
            paste_matrix_damped_track: mathutils.Matrix = paste_obj.matrix_world @ paste_bone_damped_track.matrix
            self._assert_almost_equal_lists(copy_matrix.to_translation(), paste_matrix_damped_track.to_translation())
            self.assertNotEqual(copy_matrix.to_quaternion(), paste_matrix_damped_track.to_quaternion())

            # The child of constraint is just not supported.
            paste_matrix_child_of: mathutils.Matrix = paste_obj.matrix_world @ paste_bone_child_of.matrix
            if frame > 1:
                # Frame 0 and 1 contain no rotation, so the paste works there.
                self.assertNotEqual(copy_matrix.to_translation(), paste_matrix_child_of.to_translation())
                self.assertNotEqual(copy_matrix.to_quaternion(), paste_matrix_child_of.to_quaternion())
            else:
                self.assertEqual(copy_matrix.to_translation(), paste_matrix_child_of.to_translation())
                self.assertEqual(copy_matrix.to_quaternion(), paste_matrix_child_of.to_quaternion())

    def test_paste_to_bone_chain(self) -> None:
        """When pasting to a dependent chain of bones, we have to paste in the correct order or the resulting world space would be incorrect."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_chain"]
        copy_obj.select_set(True)
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_chain"]
        paste_obj.select_set(True)

        bpy.ops.object.mode_set(mode='POSE')
        _set_select_all_bones(copy_obj, True)
        _set_select_all_bones(paste_obj, False)

        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)

        _set_select_all_bones(copy_obj, False)
        _set_select_all_bones(paste_obj, True)

        bpy.ops.anim.world_space_paste()

        for frame in range(10):
            bpy.context.scene.frame_set(frame)
            for bone_name in ["a", "b", "c"]:
                copy_bone: bpy.types.PoseBone = copy_obj.pose.bones[bone_name]
                paste_bone: bpy.types.PoseBone = paste_obj.pose.bones[bone_name]
                self._assert_almost_equal_matrix(
                    copy_obj.matrix_world @ copy_bone.matrix,
                    paste_obj.matrix_world @ paste_bone.matrix)

    def test_pasting_no_match(self) -> None:
        """Matching goes by naming. If nothing can be matched no data is pasted and an error is raised."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_chain_different_naming"]
        copy_obj.select_set(True)
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_chain"]
        paste_obj.select_set(True)

        bpy.ops.object.mode_set(mode='POSE')
        _set_select_all_bones(copy_obj, True)
        _set_select_all_bones(paste_obj, False)

        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)

        _set_select_all_bones(copy_obj, False)
        _set_select_all_bones(paste_obj, True)

        with self.assertRaises(RuntimeError):
            bpy.ops.anim.world_space_paste()

    def test_paste_360deg_euler_rotation(self) -> None:
        """Pasting euler rotations should result in a euler filtered result and not contain 360 degree jumps."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_euler"]
        copy_obj.select_set(True)
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_euler"]
        paste_obj.select_set(True)

        bpy.ops.object.mode_set(mode='POSE')
        copy_bone: bpy.types.PoseBone = copy_obj.pose.bones[0]
        paste_bone: bpy.types.PoseBone = paste_obj.pose.bones[0]
        copy_bone.select = True
        paste_bone.select = False

        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)
        copy_bone.select = False
        paste_bone.select = True

        bpy.ops.anim.world_space_paste()

        paste_anim_data: bpy.types.AnimData = paste_obj.animation_data
        assert paste_anim_data is not None
        action: bpy.types.Action = paste_anim_data.action
        # The action is created.
        assert action is not None
        channelbag = action.layers[0].strips[0].channelbags[0]
        for fcurve in channelbag.fcurves:
            prev_value = fcurve.evaluate(frame=0)
            for key in fcurve.keyframe_points:
                # The delta has to be less or equal than Pi which is a 180 degree rotation in radians.
                # Any value larger than that can be offset by 2 Pi to be closer to the previous value.
                self.assertLessEqual(abs(prev_value - key.co.y), 3.14)
                prev_value = key.co.y

    def test_paste_with_offset(self) -> None:
        """It is possible to use the current frame as the starting point for pasting data."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_simple"]
        copy_obj.select_set(True)
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_single_bone"]
        paste_obj.select_set(True)

        bpy.ops.object.mode_set(mode='POSE')
        copy_bone: bpy.types.PoseBone = copy_obj.pose.bones[0]
        paste_bone: bpy.types.PoseBone = paste_obj.pose.bones[0]
        copy_bone.select = True
        paste_bone.select = False

        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)
        copy_matrices = []
        for frame in range(10):
            bpy.context.scene.frame_set(frame)
            copy_matrices.append(copy_obj.matrix_world @ copy_bone.matrix)

        copy_bone.select = False
        paste_bone.select = True

        bpy.context.scene.frame_set(10)
        bpy.ops.anim.world_space_paste(offset='START')
        for i in range(10):
            bpy.context.scene.frame_set(i + 10)
            self._assert_almost_equal_matrix(copy_matrices[i], paste_obj.matrix_world @ paste_bone.matrix)

    def test_paste_with_delta_transform(self) -> None:
        """Objects contain an extra matrix with the delta transform.
        Pasting world spaces needs to take that into account."""
        copy_obj: bpy.types.Object = bpy.data.objects["delta_transform_object"]
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_delta_transform_object"]
        copy_obj.select_set(True)
        paste_obj.select_set(False)

        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)

        copy_obj.select_set(False)
        paste_obj.select_set(True)
        bpy.ops.anim.world_space_paste(offset='NONE')

        self._assert_objects_equal_world_space(copy_obj, paste_obj)

    def test_copy_from_animated_rotation_mode(self) -> None:
        """When copying the world space, we should switch to whatever rotation
        representation is currently active."""
        copy_obj: bpy.types.Object = bpy.data.objects["animated_rotation_mode"]
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_object"]
        copy_obj.select_set(True)
        paste_obj.select_set(False)

        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)

        copy_obj.select_set(False)
        paste_obj.select_set(True)
        bpy.ops.anim.world_space_paste(offset='NONE')

        self._assert_objects_equal_world_space(copy_obj, paste_obj)

    def test_paste_to_animated_rotation_mode(self) -> None:
        """When the rotation mode is animated, the pasting of world space
        transforms needs to use the correct mode depending on the frame."""
        copy_obj: bpy.types.Object = bpy.data.objects["delta_transform_object"]
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["animated_rotation_mode"]
        copy_obj.select_set(True)
        paste_obj.select_set(False)

        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=0, end=10)

        copy_obj.select_set(False)
        paste_obj.select_set(True)
        bpy.ops.anim.world_space_paste(offset='NONE')

        self._assert_objects_equal_world_space(copy_obj, paste_obj)


class SingleFrameCopyPasteTest(AbstractCopyPasteTest):

    def test_copy_paste_same_frame(self) -> None:
        """Easy case, pasting a single frame from one object to another."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_simple"]
        copy_obj.select_set(True)
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_single_bone"]
        paste_obj.select_set(True)

        bpy.ops.object.mode_set(mode='POSE')
        copy_bone: bpy.types.PoseBone = copy_obj.pose.bones[0]
        paste_bone: bpy.types.PoseBone = paste_obj.pose.bones[0]
        copy_bone.select = True
        paste_bone.select = False

        bpy.context.scene.frame_set(1)
        # This copies frame 10 even though we are not currently on that frame.
        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=10, end=11)

        copy_bone.select = False
        paste_bone.select = True
        bpy.ops.anim.world_space_paste(offset='NONE')

        bpy.context.scene.frame_set(10)

        self._assert_almost_equal_matrix(
            copy_obj.matrix_world @ copy_bone.matrix,
            paste_obj.matrix_world @ paste_bone.matrix)

    def test_copy_paste_frame_offset(self) -> None:
        """Test pasting data on a different frame than where it was copied from."""
        copy_obj: bpy.types.Object = bpy.data.objects["armature_simple"]
        copy_obj.select_set(True)
        bpy.context.view_layer.objects.active = copy_obj
        paste_obj: bpy.types.Object = bpy.data.objects["paste_armature_single_bone"]
        paste_obj.select_set(True)

        bpy.ops.object.mode_set(mode='POSE')
        copy_bone: bpy.types.PoseBone = copy_obj.pose.bones[0]
        paste_bone: bpy.types.PoseBone = paste_obj.pose.bones[0]
        copy_bone.select = True
        paste_bone.select = False

        bpy.context.scene.frame_set(10)
        copy_matrix: mathutils.Matrix = copy_obj.matrix_world @ copy_bone.matrix
        bpy.ops.anim.world_space_copy(range_mode='CUSTOM', start=10, end=11)

        bpy.context.scene.frame_set(1)
        copy_bone.select = False
        paste_bone.select = True
        bpy.ops.anim.world_space_paste(offset='START')

        self._assert_almost_equal_matrix(copy_matrix, paste_obj.matrix_world @ paste_bone.matrix)


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
