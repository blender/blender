# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import bpy
import pathlib
import sys
from math import radians

"""
blender -b -noaudio --factory-startup --python tests/python/bl_animation_keyframing.py -- --testdir /path/to/lib/tests/animation
"""


def _fcurve_paths_match(fcurves: list, expected_paths: list) -> bool:
    data_paths = list(set([fcurve.data_path for fcurve in fcurves]))
    return sorted(data_paths) == sorted(expected_paths)


def _get_view3d_context():
    ctx = bpy.context.copy()

    for area in bpy.context.window.screen.areas:
        if area.type != 'VIEW_3D':
            continue

        ctx['area'] = area
        ctx['space'] = area.spaces.active
        break

    return ctx


def _create_animation_object():
    anim_object = bpy.data.objects.new("anim_object", None)
    # Ensure that the rotation mode is correct so we can check against rotation_euler
    anim_object.rotation_mode = "XYZ"
    bpy.context.scene.collection.objects.link(anim_object)
    bpy.context.view_layer.objects.active = anim_object
    anim_object.select_set(True)
    return anim_object


_BONE_NAME = "bone"


def _create_armature():
    armature = bpy.data.armatures.new("anim_armature")
    armature_obj = bpy.data.objects.new("anim_object", armature)
    bpy.context.scene.collection.objects.link(armature_obj)
    bpy.context.view_layer.objects.active = armature_obj
    armature_obj.select_set(True)

    bpy.ops.object.mode_set(mode='EDIT')
    edit_bone = armature.edit_bones.new(_BONE_NAME)
    edit_bone.head = (1, 0, 0)
    bpy.ops.object.mode_set(mode='POSE')
    armature_obj.pose.bones[_BONE_NAME].rotation_mode = "XYZ"
    armature_obj.pose.bones[_BONE_NAME].bone.select = True
    armature_obj.pose.bones[_BONE_NAME].bone.select_head = True
    armature_obj.pose.bones[_BONE_NAME].bone.select_tail = True
    bpy.ops.object.mode_set(mode='OBJECT')

    return armature_obj


def _insert_by_name_test(insert_key: str, expected_paths: list):
    keyed_object = _create_animation_object()
    with bpy.context.temp_override(**_get_view3d_context()):
        bpy.ops.anim.keyframe_insert_by_name(type=insert_key)
    match = _fcurve_paths_match(keyed_object.animation_data.action.fcurves, expected_paths)
    bpy.data.objects.remove(keyed_object, do_unlink=True)
    return match


def _insert_from_user_preference_test(enabled_user_pref_fields: set, expected_paths: list):
    keyed_object = _create_animation_object()
    bpy.context.preferences.edit.key_insert_channels = enabled_user_pref_fields
    with bpy.context.temp_override(**_get_view3d_context()):
        bpy.ops.anim.keyframe_insert()
    match = _fcurve_paths_match(keyed_object.animation_data.action.fcurves, expected_paths)
    bpy.data.objects.remove(keyed_object, do_unlink=True)
    return match


def _get_keying_set(scene, name: str):
    return scene.keying_sets_all[scene.keying_sets_all.find(name)]


def _insert_with_keying_set_test(keying_set_name: str, expected_paths: list):
    scene = bpy.context.scene
    keying_set = _get_keying_set(scene, keying_set_name)
    scene.keying_sets.active = keying_set
    keyed_object = _create_animation_object()
    with bpy.context.temp_override(**_get_view3d_context()):
        bpy.ops.anim.keyframe_insert()
    match = _fcurve_paths_match(keyed_object.animation_data.action.fcurves, expected_paths)
    bpy.data.objects.remove(keyed_object, do_unlink=True)
    return match


class AbstractKeyframingTest:
    def setUp(self):
        super().setUp()
        bpy.ops.wm.read_homefile(use_factory_startup=True)


class InsertKeyTest(AbstractKeyframingTest, unittest.TestCase):
    """ Ensure keying things by name or with a keying set adds the right keys. """

    def test_insert_by_name(self):
        self.assertTrue(_insert_by_name_test("Location", ["location"]))
        self.assertTrue(_insert_by_name_test("Rotation", ["rotation_euler"]))
        self.assertTrue(_insert_by_name_test("Scaling", ["scale"]))
        self.assertTrue(_insert_by_name_test("LocRotScale", ["location", "rotation_euler", "scale"]))

    def test_insert_with_keying_set(self):
        self.assertTrue(_insert_with_keying_set_test("Location", ["location"]))
        self.assertTrue(_insert_with_keying_set_test("Rotation", ["rotation_euler"]))
        self.assertTrue(_insert_with_keying_set_test("Scale", ["scale"]))
        self.assertTrue(
            _insert_with_keying_set_test("Location, Rotation & Scale", ["location", "rotation_euler", "scale"])
        )

    def test_insert_from_user_preferences(self):
        self.assertTrue(_insert_from_user_preference_test({"LOCATION"}, ["location"]))
        self.assertTrue(_insert_from_user_preference_test({"ROTATION"}, ["rotation_euler"]))
        self.assertTrue(_insert_from_user_preference_test({"SCALE"}, ["scale"]))
        self.assertTrue(_insert_from_user_preference_test(
            {"LOCATION", "ROTATION", "SCALE"}, ["location", "rotation_euler", "scale"]))


class VisualKeyingTest(AbstractKeyframingTest, unittest.TestCase):
    """ Check if visual keying produces the correct keyframe values. """

    def test_visual_location_keying_set(self):
        t_value = 1
        target = _create_animation_object()
        target.location = (t_value, t_value, t_value)
        constrained = _create_animation_object()
        constraint = constrained.constraints.new("COPY_LOCATION")
        constraint.target = target

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert_by_name(type="BUILTIN_KSI_VisualLoc")

        for fcurve in constrained.animation_data.action.fcurves:
            self.assertEqual(fcurve.keyframe_points[0].co.y, t_value)

        bpy.data.objects.remove(target, do_unlink=True)
        bpy.data.objects.remove(constrained, do_unlink=True)

    def test_visual_rotation_keying_set(self):
        rot_value_deg = 45
        rot_value_rads = radians(rot_value_deg)

        target = _create_animation_object()
        target.rotation_euler = (rot_value_rads, rot_value_rads, rot_value_rads)
        constrained = _create_animation_object()
        constraint = constrained.constraints.new("COPY_ROTATION")
        constraint.target = target

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert_by_name(type="BUILTIN_KSI_VisualRot")

        for fcurve in constrained.animation_data.action.fcurves:
            self.assertAlmostEqual(fcurve.keyframe_points[0].co.y, rot_value_rads, places=4)

        bpy.data.objects.remove(target, do_unlink=True)
        bpy.data.objects.remove(constrained, do_unlink=True)

    def test_visual_location_user_pref_override(self):
        # When enabling the user preference setting,
        # the normal keying sets behave like their visual keying set counterpart.
        bpy.context.preferences.edit.use_visual_keying = True
        t_value = 1
        target = _create_animation_object()
        target.location = (t_value, t_value, t_value)
        constrained = _create_animation_object()
        constraint = constrained.constraints.new("COPY_LOCATION")
        constraint.target = target

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert_by_name(type="Location")

        for fcurve in constrained.animation_data.action.fcurves:
            self.assertEqual(fcurve.keyframe_points[0].co.y, t_value)

        bpy.data.objects.remove(target, do_unlink=True)
        bpy.data.objects.remove(constrained, do_unlink=True)
        bpy.context.preferences.edit.use_visual_keying = False

    def test_visual_location_user_pref(self):
        target = _create_animation_object()
        t_value = 1
        target.location = (t_value, t_value, t_value)
        constrained = _create_animation_object()
        constraint = constrained.constraints.new("COPY_LOCATION")
        constraint.target = target

        bpy.context.preferences.edit.use_visual_keying = True
        bpy.context.preferences.edit.key_insert_channels = {"LOCATION"}

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert()

        for fcurve in constrained.animation_data.action.fcurves:
            self.assertEqual(fcurve.keyframe_points[0].co.y, t_value)

        bpy.data.objects.remove(target, do_unlink=True)
        bpy.data.objects.remove(constrained, do_unlink=True)
        bpy.context.preferences.edit.use_visual_keying = False


class CycleAwareKeyingTest(AbstractKeyframingTest, unittest.TestCase):
    """ Check if cycle aware keying remaps the keyframes correctly and adds fcurve modifiers. """

    def test_insert_location_cycle_aware(self):
        # In order to make cycle aware keying work, the action needs to be created and have the
        # frame_range set plus the use_frame_range flag set to True.
        keyed_object = _create_animation_object()
        bpy.context.scene.tool_settings.use_keyframe_cycle_aware = True

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert_by_name(type="Location")

        action = keyed_object.animation_data.action
        action.use_cyclic = True
        action.use_frame_range = True
        cyclic_range_end = 20
        action.frame_range = [1, cyclic_range_end]

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(5)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")

            # Will be mapped to frame 3.
            # This will insert the key based on the user preference settings.
            bpy.context.preferences.edit.key_insert_channels = {"LOCATION"}
            bpy.context.scene.frame_set(22)
            bpy.ops.anim.keyframe_insert()

            # Will be mapped to frame 9.
            bpy.context.scene.frame_set(-10)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")

        # Check that only location keys have been created.
        self.assertTrue(_fcurve_paths_match(action.fcurves, ["location"]))

        expected_keys = [1, 3, 5, 9, 20]

        for fcurve in action.fcurves:
            self.assertEqual(len(fcurve.keyframe_points), len(expected_keys))
            key_index = 0
            for key in fcurve.keyframe_points:
                self.assertEqual(key.co.x, expected_keys[key_index])
                key_index += 1

            # All fcurves should have a cycles modifier.
            self.assertTrue(fcurve.modifiers[0].type == "CYCLES")

        bpy.data.objects.remove(keyed_object, do_unlink=True)


class AutoKeyframingTest(AbstractKeyframingTest, unittest.TestCase):

    def test_autokey_basic(self):
        keyed_object = _create_animation_object()
        bpy.context.scene.tool_settings.use_keyframe_insert_auto = True
        bpy.context.preferences.edit.use_keyframe_insert_available = False
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.transform.translate(value=(1, 0, 0))

        action = keyed_object.animation_data.action
        self.assertTrue(_fcurve_paths_match(action.fcurves, ["location", "rotation_euler", "scale"]))
        bpy.data.objects.remove(keyed_object, do_unlink=True)

    def test_autokey_available(self):
        keyed_object = _create_animation_object()

        bpy.context.scene.tool_settings.use_keyframe_insert_auto = True
        bpy.context.preferences.edit.use_keyframe_insert_available = True

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(1)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")
            bpy.context.scene.frame_set(5)
            bpy.ops.transform.translate(value=(1, 0, 0))

        action = keyed_object.animation_data.action
        self.assertTrue(_fcurve_paths_match(action.fcurves, ["location"]))

        for fcurve in action.fcurves:
            self.assertEqual(len(fcurve.keyframe_points), 2)

        bpy.context.preferences.edit.use_keyframe_insert_available = False
        bpy.data.objects.remove(keyed_object, do_unlink=True)

    def test_autokey_bone(self):
        armature_obj = _create_armature()
        bpy.context.scene.tool_settings.use_keyframe_insert_auto = True
        bpy.context.preferences.edit.use_keyframe_insert_available = False

        bpy.ops.object.mode_set(mode='POSE')
        # Not overriding the context because it would mean context.selected_pose_bones is empty
        # resulting in a failure to move/key the bone
        bpy.ops.transform.translate(value=(1, 0, 0))
        bpy.ops.object.mode_set(mode='OBJECT')

        action = armature_obj.animation_data.action
        bone_path = f"pose.bones[\"{_BONE_NAME}\"]"
        expected_paths = [f"{bone_path}.location", f"{bone_path}.rotation_euler", f"{bone_path}.scale"]
        self.assertTrue(_fcurve_paths_match(action.fcurves, expected_paths))

        bpy.data.objects.remove(armature_obj, do_unlink=True)


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
