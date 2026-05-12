# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy_extras import anim_utils

import unittest
import sys
import pathlib

"""
blender -b --factory-startup --python tests/python/bl_animation_pose_slide.py
"""


def _get_view3d_context():
    ctx = bpy.context.copy()

    for area in bpy.context.window.screen.areas:
        if area.type != 'VIEW_3D':
            continue

        ctx['area'] = area
        ctx['space'] = area.spaces.active
        break

    return ctx


_BONE_NAME = "bone"
_CUSTOM_PROP = "test"


class AbstractPoseSlideTest(unittest.TestCase):
    armature_ob: bpy.types.Object
    pose_bone: bpy.types.PoseBone

    def _keyframe_all(self):
        self.pose_bone.keyframe_insert("location")
        self.pose_bone.keyframe_insert("rotation_euler")
        self.pose_bone.keyframe_insert("scale")
        self.pose_bone.keyframe_insert("bbone_curveinx")
        self.pose_bone.keyframe_insert(f'["{_CUSTOM_PROP}"]')

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        armature = bpy.data.armatures.new("armature")
        self.armature_ob = bpy.data.objects.new("armature_ob", armature)
        bpy.context.scene.collection.objects.link(self.armature_ob)
        bpy.context.view_layer.objects.active = self.armature_ob
        bpy.ops.object.mode_set(mode='EDIT')
        armature.edit_bones.new(_BONE_NAME)
        bpy.ops.object.mode_set(mode='POSE')
        self.pose_bone = self.armature_ob.pose.bones[_BONE_NAME]
        self.pose_bone.rotation_mode = 'XYZ'
        self.pose_bone[_CUSTOM_PROP] = 1.0
        self.pose_bone.select = True


class BlendToDefaultPoseBone(AbstractPoseSlideTest):

    def test_all_properties(self):
        bpy.context.scene.frame_set(0)
        self.pose_bone.location = (1, 1, 1)
        self.pose_bone.rotation_euler = (1, 1, 1)
        self.pose_bone.scale = (1, 1, 1)
        self.pose_bone.bbone_curveinx = 1
        self.pose_bone[_CUSTOM_PROP] = 1.0
        self._keyframe_all()

        bpy.context.scene.frame_set(10)
        self.pose_bone.location = (2, 2, 2)
        self.pose_bone.rotation_euler = (2, 2, 2)
        self.pose_bone.scale = (2, 2, 2)
        self.pose_bone.bbone_curveinx = 2
        self.pose_bone[_CUSTOM_PROP] = 2.0
        self._keyframe_all()

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.pose.blend_with_rest(factor=1.0)

        for i in range(3):
            self.assertAlmostEqual(self.pose_bone.location[i], 0, 3)
            self.assertAlmostEqual(self.pose_bone.rotation_euler[i], 0, 3)
            self.assertAlmostEqual(self.pose_bone.scale[i], 1, 3)

        # Custom properties and bbone properties are not supported by this operator.
        self.assertAlmostEqual(self.pose_bone.bbone_curveinx, 2, 3)
        self.assertAlmostEqual(self.pose_bone[_CUSTOM_PROP], 2, 3)

    def test_lock_x_axis(self):
        bpy.context.scene.frame_set(0)
        self.pose_bone.location = (1, 1, 1)
        self._keyframe_all()

        bpy.context.scene.frame_set(10)
        self.pose_bone.location = (2, 2, 2)
        self._keyframe_all()

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.pose.blend_with_rest(factor=1.0, channels='LOC', axis_lock='X')

        self.assertAlmostEqual(self.pose_bone.location[0], 0, 3)
        self.assertAlmostEqual(self.pose_bone.location[1], 2, 3)
        self.assertAlmostEqual(self.pose_bone.location[2], 2, 3)


class BlendToNeighborPoseBone(AbstractPoseSlideTest):

    def test_all_properties(self):
        bpy.context.scene.frame_set(0)
        self.pose_bone.location = (1, 1, 1)
        self.pose_bone.rotation_euler = (1, 1, 1)
        self.pose_bone.scale = (1, 1, 1)
        self.pose_bone.bbone_curveinx = 1
        self.pose_bone[_CUSTOM_PROP] = 1.0
        self._keyframe_all()

        bpy.context.scene.frame_set(10)
        self.pose_bone.location = (2, 2, 2)
        self.pose_bone.rotation_euler = (2, 2, 2)
        self.pose_bone.scale = (2, 2, 2)
        self.pose_bone.bbone_curveinx = 2
        self.pose_bone[_CUSTOM_PROP] = 2.0
        self._keyframe_all()

        bpy.context.scene.frame_set(1)
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.pose.blend_to_neighbor(factor=1.0, prev_frame=0, next_frame=10)

        for i in range(3):
            self.assertAlmostEqual(self.pose_bone.location[i], 2, 3)
            self.assertAlmostEqual(self.pose_bone.rotation_euler[i], 2, 3)
            self.assertAlmostEqual(self.pose_bone.scale[i], 2, 3)

        self.assertAlmostEqual(self.pose_bone.bbone_curveinx, 2, 3)
        self.assertAlmostEqual(self.pose_bone[_CUSTOM_PROP], 2, 3)


class PushRelaxPoseBone(AbstractPoseSlideTest):

    def _set_up_keys(self):
        bpy.context.scene.frame_set(0)
        self.pose_bone.location = (1, 1, 1)
        self.pose_bone.rotation_euler = (1, 1, 1)
        self.pose_bone.scale = (1, 1, 1)
        self.pose_bone.bbone_curveinx = 1
        self.pose_bone[_CUSTOM_PROP] = 1.0
        self._keyframe_all()

        bpy.context.scene.frame_set(10)
        self.pose_bone.location = (2, 2, 2)
        self.pose_bone.rotation_euler = (2, 2, 2)
        self.pose_bone.scale = (2, 2, 2)
        self.pose_bone.bbone_curveinx = 2
        self.pose_bone[_CUSTOM_PROP] = 2.0
        self._keyframe_all()

        bpy.context.scene.frame_set(5)
        self.pose_bone.location = (5, 5, 5)
        self.pose_bone.rotation_euler = (5, 5, 5)
        self.pose_bone.scale = (5, 5, 5)
        self.pose_bone.bbone_curveinx = 5
        self.pose_bone[_CUSTOM_PROP] = 5.0
        self._keyframe_all()

    def test_relax_all_properties(self):
        # Relax moves the pose to the linear breakdown between the given frames.
        self._set_up_keys()

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.pose.relax(factor=1.0, prev_frame=0, next_frame=10)

        for i in range(3):
            self.assertAlmostEqual(self.pose_bone.location[i], 1.5, 3)
            self.assertAlmostEqual(self.pose_bone.rotation_euler[i], 1.5, 3)
            self.assertAlmostEqual(self.pose_bone.scale[i], 1.5, 3)

        self.assertAlmostEqual(self.pose_bone.bbone_curveinx, 1.5, 3)
        self.assertAlmostEqual(self.pose_bone[_CUSTOM_PROP], 1.5, 3)

    def test_push_all_properties(self):
        # Push moves all properties away from the linear breakdown between the given frames.
        # The distance moved at factor 1 depends on the inital distance from the breakdown pose.
        self._set_up_keys()

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.pose.push(factor=1.0, prev_frame=0, next_frame=10)

        # Math to illustrate how the result is computed.
        breakdown_value = 1 * 0.5 + 2 * 0.5
        start_value = 5
        expected_value = start_value + (start_value - breakdown_value)
        for i in range(3):
            self.assertAlmostEqual(self.pose_bone.location[i], expected_value, 3)
            self.assertAlmostEqual(self.pose_bone.rotation_euler[i], expected_value, 3)
            self.assertAlmostEqual(self.pose_bone.scale[i], expected_value, 3)

        self.assertAlmostEqual(self.pose_bone.bbone_curveinx, expected_value, 3)
        self.assertAlmostEqual(self.pose_bone[_CUSTOM_PROP], expected_value, 3)


class BreakdownerTestPoseBone(AbstractPoseSlideTest):

    def setUp(self) -> None:
        super().setUp()
        bpy.context.preferences.edit.use_keyframe_insert_available = False

    def tearDown(self) -> None:
        super().tearDown()
        bpy.context.preferences.edit.use_keyframe_insert_available = True

    def test_no_keys(self):
        # The case of no keys will produce no interpolation.
        self.pose_bone.location = (1, 1, 1)
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.pose.breakdown(factor=0.5)

        for i in range(3):
            self.assertAlmostEqual(self.pose_bone.location[i], 1, 3)

    def test_single_key(self):
        # Creating a breakdown with a single key will not change the current value
        # but adds a key if autokeying is enabled.
        bpy.context.scene.frame_set(0)
        self.pose_bone.location = (1, 1, 1)
        self.pose_bone.keyframe_insert("location")

        bpy.context.scene.frame_set(10)

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.pose.breakdown(factor=0.5)

        action = self.armature_ob.animation_data.action
        slot = self.armature_ob.animation_data.action_slot
        channelbag = anim_utils.action_get_channelbag_for_slot(action, slot)
        self.assertEqual(len(channelbag.fcurves), 3)
        self.assertEqual(len(channelbag.fcurves[0].keyframe_points), 1)

        for i in range(3):
            self.assertAlmostEqual(self.pose_bone.location[i], 1, 3)

        bpy.context.scene.tool_settings.use_keyframe_insert_auto = True

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.pose.breakdown(factor=0.5)

        for i in range(3):
            self.assertAlmostEqual(self.pose_bone.location[i], 1, 3)

        # The key count depends on the setting "Only insert available" in the user preferences.
        self.assertEqual(len(channelbag.fcurves), 10)
        self.assertEqual(len(channelbag.fcurves[0].keyframe_points), 2)

    def test_all_properties(self):
        # By default the pose slide operators act on location, rotation, scale, bbone properties and custom properties.
        bpy.context.scene.frame_set(0)
        self.pose_bone.location = (1, 1, 1)
        self.pose_bone.rotation_euler = (1, 1, 1)
        self.pose_bone.scale = (1, 1, 1)
        self.pose_bone.bbone_curveinx = 1
        self.pose_bone[_CUSTOM_PROP] = 1.0
        self._keyframe_all()

        bpy.context.scene.frame_set(10)
        self.pose_bone.location = (2, 2, 2)
        self.pose_bone.rotation_euler = (2, 2, 2)
        self.pose_bone.scale = (2, 2, 2)
        self.pose_bone.bbone_curveinx = 2
        self.pose_bone[_CUSTOM_PROP] = 2.0
        self._keyframe_all()

        action = self.armature_ob.animation_data.action
        slot = self.armature_ob.animation_data.action_slot
        channelbag = anim_utils.action_get_channelbag_for_slot(action, slot)
        self.assertEqual(len(channelbag.fcurves), 11)

        bpy.context.scene.tool_settings.use_keyframe_insert_auto = True

        bpy.context.scene.frame_set(1)
        with bpy.context.temp_override(**_get_view3d_context()):
            # The prev and next frames have to be specified in order for this to work correctly.
            bpy.ops.pose.breakdown(factor=0.5, prev_frame=0, next_frame=10)

        for i in range(3):
            self.assertAlmostEqual(self.pose_bone.location[i], 1.5, 3)
            self.assertAlmostEqual(self.pose_bone.rotation_euler[i], 1.5, 3)
            self.assertAlmostEqual(self.pose_bone.scale[i], 1.5, 3)

        self.assertAlmostEqual(self.pose_bone.bbone_curveinx, 1.5, 3)
        self.assertAlmostEqual(self.pose_bone[_CUSTOM_PROP], 1.5, 3)

    def test_location(self):
        # The pose slide operators can constrain to a single property type.
        # All other properties should not be modified.
        bpy.context.scene.frame_set(0)
        self.pose_bone.location = (1, 1, 1)
        self.pose_bone.rotation_euler = (1, 1, 1)
        self._keyframe_all()

        bpy.context.scene.frame_set(10)
        self.pose_bone.location = (2, 2, 2)
        self.pose_bone.rotation_euler = (2, 2, 2)
        self._keyframe_all()

        bpy.context.scene.tool_settings.use_keyframe_insert_auto = True

        bpy.context.scene.frame_set(1)
        with bpy.context.temp_override(**_get_view3d_context()):
            # The prev and next frames have to be specified in order for this to work correctly.
            bpy.ops.pose.breakdown(factor=0.5, prev_frame=0, next_frame=10, channels='LOC')

        for i in range(3):
            self.assertAlmostEqual(self.pose_bone.location[i], 1.5, 3)
            # Rotation should not be modified.
            self.assertAlmostEqual(self.pose_bone.rotation_euler[i], 1.0279, 3)

    def test_location_x(self):
        # The slider operators can constrain to a single axis.
        bpy.context.scene.frame_set(0)
        self.pose_bone.location = (1, 1, 1)
        self._keyframe_all()

        bpy.context.scene.frame_set(10)
        self.pose_bone.location = (2, 2, 2)
        self._keyframe_all()

        bpy.context.scene.frame_set(1)
        with bpy.context.temp_override(**_get_view3d_context()):
            # The prev and next frames have to be specified in order for this to work correctly.
            bpy.ops.pose.breakdown(factor=0.5, prev_frame=0, next_frame=10, channels='LOC', axis_lock='X')

        self.assertAlmostEqual(self.pose_bone.location[0], 1.5, 3)
        self.assertAlmostEqual(self.pose_bone.location[1], 1.0279, 3)
        self.assertAlmostEqual(self.pose_bone.location[2], 1.0279, 3)

    def test_quaternion_axis_lock_limitation(self):
        # Axis lock does not work with quaternions. This test just confirms that.
        self.pose_bone.rotation_mode = 'QUATERNION'
        bpy.context.scene.frame_set(0)
        self.pose_bone.rotation_quaternion = (1, 0, 0, 0)
        self.pose_bone.keyframe_insert("rotation_quaternion")

        bpy.context.scene.frame_set(10)
        self.pose_bone.rotation_quaternion = (0, 1, 0, 0)
        self.pose_bone.keyframe_insert("rotation_quaternion")

        with bpy.context.temp_override(**_get_view3d_context()):
            # The prev and next frames have to be specified in order for this to work correctly.
            bpy.ops.pose.breakdown(factor=0.0, prev_frame=0, next_frame=10, channels='ROT', axis_lock='X')

        # Despite setting the axis lock, all channels are modified.
        self.assertAlmostEqual(self.pose_bone.rotation_quaternion[0], 1, 3)
        self.assertAlmostEqual(self.pose_bone.rotation_quaternion[1], 0, 3)
        self.assertAlmostEqual(self.pose_bone.rotation_quaternion[2], 0, 3)
        self.assertAlmostEqual(self.pose_bone.rotation_quaternion[3], 0, 3)


if __name__ == "__main__":
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
