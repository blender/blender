# SPDX-FileCopyrightText: 2020-2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import math
import unittest
import bpy
import sys
import pathlib


class MotionPathTestObject(unittest.TestCase):

    anim_object: bpy.types.Object

    def setUp(self) -> None:
        super().setUp()
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        self.anim_object = bpy.data.objects.new("anim_object", None)
        bpy.context.scene.collection.objects.link(self.anim_object)
        bpy.context.view_layer.objects.active = self.anim_object
        self.anim_object.select_set(True)

    def test_cache_range(self):
        """Testing if the motion path creates the correct range."""
        self.anim_object.keyframe_insert("location", frame=0)
        self.anim_object.keyframe_insert("location", frame=10)

        bpy.ops.object.paths_calculate(range='SCENE')
        self.assertNotEqual(self.anim_object.motion_path, None)
        motion_path = self.anim_object.motion_path
        self.assertEqual(motion_path.frame_start, bpy.context.scene.frame_start)
        # The motion path frame_end is exclusive while the scene frame_end is inclusive.
        self.assertEqual(motion_path.frame_end, bpy.context.scene.frame_end + 1)

        bpy.ops.object.paths_calculate(range='KEYS_ALL')
        self.assertEqual(motion_path.frame_start, 0)
        self.assertEqual(motion_path.frame_end, 11)

        bpy.ops.object.paths_calculate(range='KEYS_SELECTED')
        # Both keys are selected.
        self.assertEqual(motion_path.frame_start, 0)
        self.assertEqual(motion_path.frame_end, 11)

        self.anim_object.animation_visualization.motion_path.frame_start = 3
        # frame_end is inclusive.
        self.anim_object.animation_visualization.motion_path.frame_end = 6
        bpy.ops.object.paths_calculate(range='MANUAL')
        self.assertEqual(motion_path.frame_start, 3)
        self.assertEqual(motion_path.frame_end, 7)

    def test_motion_path_of_child(self):
        """Motion paths are in world space so they reflect the movement of the parent as well."""
        parent_ob = bpy.data.objects.new("parent", None)
        bpy.context.scene.collection.objects.link(parent_ob)
        parent_ob.location = (1, 0, 0)
        self.anim_object.parent = parent_ob
        # Needs to be evaluated for the matrix properties to return the correct value.
        bpy.context.evaluated_depsgraph_get()
        self.anim_object.matrix_parent_inverse = parent_ob.matrix_local.inverted()
        parent_ob.rotation_euler = (0, math.pi / 2, 0)
        parent_ob.keyframe_insert("rotation_euler", frame=1)
        parent_ob.rotation_euler = (0, 0, 0)
        parent_ob.keyframe_insert("rotation_euler", frame=10)

        bpy.ops.object.paths_calculate(range='SCENE')
        motion_path = self.anim_object.motion_path
        self.assertAlmostEqual(motion_path.points[0].co[0], 1, 3)
        self.assertAlmostEqual(motion_path.points[0].co[1], 0, 3)
        self.assertAlmostEqual(motion_path.points[0].co[2], 1, 3)

        self.assertAlmostEqual(motion_path.points[-1].co[0], 0, 3)
        self.assertAlmostEqual(motion_path.points[-1].co[1], 0, 3)
        self.assertAlmostEqual(motion_path.points[-1].co[2], 0, 3)


class MotionPathTestArmature(unittest.TestCase):
    anim_armature_object: bpy.types.Object
    pose_bone_a: bpy.types.PoseBone
    pose_bone_b: bpy.types.PoseBone

    def setUp(self) -> None:
        super().setUp()
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        armature = bpy.data.armatures.new("anim_armature")
        self.anim_armature_object = bpy.data.objects.new("anim_armature_ob", armature)
        bpy.context.scene.collection.objects.link(self.anim_armature_object)
        bpy.context.view_layer.objects.active = self.anim_armature_object
        self.anim_armature_object.select_set(True)

        bone_name_a = "bone"
        bone_name_b = "bone_2"
        bpy.ops.object.mode_set(mode='EDIT')
        edit_bone = armature.edit_bones.new(bone_name_a)
        edit_bone.head = (1, 0, 0)
        edit_bone = armature.edit_bones.new(bone_name_b)
        edit_bone.head = (1, 1, 0)
        edit_bone.tail = (0, 1, 0)

        bpy.ops.object.mode_set(mode='POSE')
        self.pose_bone_a = self.anim_armature_object.pose.bones[bone_name_a]
        self.pose_bone_a.select = True
        self.pose_bone_b = self.anim_armature_object.pose.bones[bone_name_b]
        # Second bone is not selected by default. Should get no motion path.
        self.pose_bone_b.select = False

    def test_cache_range(self):
        """Testing if the motion path creates the correct range."""
        self.pose_bone_a.keyframe_insert("location", frame=0)
        self.pose_bone_a.keyframe_insert("location", frame=10)

        bpy.ops.pose.paths_calculate(range='SCENE')
        self.assertNotEqual(self.pose_bone_a.motion_path, None)
        self.assertEqual(
            self.pose_bone_b.motion_path,
            None,
            "The unselected bone should get no motion path.")
        motion_path = self.pose_bone_a.motion_path

        self.assertEqual(motion_path.frame_start, bpy.context.scene.frame_start)
        # The motion path frame_end is exclusive while the scene frame_end is inclusive.
        self.assertEqual(motion_path.frame_end, bpy.context.scene.frame_end + 1)

        bpy.ops.pose.paths_calculate(range='KEYS_ALL')
        self.assertEqual(motion_path.frame_start, 0)
        self.assertEqual(motion_path.frame_end, 11)

        bpy.ops.pose.paths_calculate(range='KEYS_SELECTED')
        # Both keys are selected.
        self.assertEqual(motion_path.frame_start, 0)
        self.assertEqual(motion_path.frame_end, 11)

        self.anim_armature_object.pose.animation_visualization.motion_path.frame_start = 3
        # frame_end is inclusive.
        self.anim_armature_object.pose.animation_visualization.motion_path.frame_end = 6
        bpy.ops.pose.paths_calculate(range='MANUAL')
        self.assertEqual(motion_path.frame_start, 3)
        self.assertEqual(motion_path.frame_end, 7)

    def test_bake_head_tail(self):
        """Bones can bake either at the head or the tail."""
        self.pose_bone_a.keyframe_insert("location", frame=10)
        self.pose_bone_a.location = (1, 0, 0)
        self.pose_bone_a.keyframe_insert("location", frame=0)
        bpy.ops.pose.paths_calculate(range='KEYS_ALL', bake_location='HEADS')

        motion_path = self.pose_bone_a.motion_path
        for point in motion_path.points:
            self.assertAlmostEqual(point.co[0], 1, 3)

        # If we don't clear the path, the bake option has no effect.
        # I (christoph) think that behavior should change, but it is documented here anyway.
        bpy.ops.pose.paths_calculate(range='KEYS_ALL', bake_location='TAILS')
        self.assertEqual(len(motion_path.points), 11)
        # Since the animation is a movement on the local x-axis, which equals to
        # the global y in this setup the x-value will not change throughout the
        # animation.
        for point in motion_path.points:
            self.assertAlmostEqual(point.co[0], 1, 3)

        bpy.ops.pose.paths_clear(only_selected=True)
        self.assertEqual(self.pose_bone_a.motion_path, None)
        bpy.ops.pose.paths_calculate(range='KEYS_ALL', bake_location='TAILS')
        motion_path = self.pose_bone_a.motion_path
        self.assertEqual(len(motion_path.points), 11)
        for point in motion_path.points:
            self.assertAlmostEqual(point.co[0], 0, 3)


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
