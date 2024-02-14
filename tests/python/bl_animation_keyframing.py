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
    data_paths.sort()
    expected_paths.sort()
    if data_paths != expected_paths:
        raise AssertionError(
            f"Expected paths do not match F-Curve paths. Expected: {expected_paths}. F-Curve: {data_paths}")


def _get_view3d_context():
    ctx = bpy.context.copy()

    for area in bpy.context.window.screen.areas:
        if area.type != 'VIEW_3D':
            continue

        ctx['area'] = area
        ctx['space'] = area.spaces.active
        break

    return ctx


def _get_nla_context():
    ctx = bpy.context.copy()

    for area in bpy.context.window.screen.areas:
        if area.type != 'NLA_EDITOR':
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
    _fcurve_paths_match(keyed_object.animation_data.action.fcurves, expected_paths)
    bpy.data.objects.remove(keyed_object, do_unlink=True)


def _insert_from_user_preference_test(enabled_user_pref_fields: set, expected_paths: list):
    keyed_object = _create_animation_object()
    bpy.context.preferences.edit.key_insert_channels = enabled_user_pref_fields
    with bpy.context.temp_override(**_get_view3d_context()):
        bpy.ops.anim.keyframe_insert()
    _fcurve_paths_match(keyed_object.animation_data.action.fcurves, expected_paths)
    bpy.data.objects.remove(keyed_object, do_unlink=True)


def _get_keying_set(scene, name: str):
    return scene.keying_sets_all[scene.keying_sets_all.find(name)]


def _insert_with_keying_set_test(keying_set_name: str, expected_paths: list):
    scene = bpy.context.scene
    keying_set = _get_keying_set(scene, keying_set_name)
    scene.keying_sets.active = keying_set
    keyed_object = _create_animation_object()
    with bpy.context.temp_override(**_get_view3d_context()):
        bpy.ops.anim.keyframe_insert()
    _fcurve_paths_match(keyed_object.animation_data.action.fcurves, expected_paths)
    bpy.data.objects.remove(keyed_object, do_unlink=True)


class AbstractKeyframingTest:
    def setUp(self):
        super().setUp()
        bpy.ops.wm.read_homefile(use_factory_startup=True)


class InsertKeyTest(AbstractKeyframingTest, unittest.TestCase):
    """ Ensure keying things by name or with a keying set adds the right keys. """

    def test_insert_by_name(self):
        _insert_by_name_test("Location", ["location"])
        _insert_by_name_test("Rotation", ["rotation_euler"])
        _insert_by_name_test("Scaling", ["scale"])
        _insert_by_name_test("LocRotScale", ["location", "rotation_euler", "scale"])

    def test_insert_with_keying_set(self):
        _insert_with_keying_set_test("Location", ["location"])
        _insert_with_keying_set_test("Rotation", ["rotation_euler"])
        _insert_with_keying_set_test("Scale", ["scale"])
        _insert_with_keying_set_test("Location, Rotation & Scale", ["location", "rotation_euler", "scale"])

    def test_insert_from_user_preferences(self):
        _insert_from_user_preference_test({"LOCATION"}, ["location"])
        _insert_from_user_preference_test({"ROTATION"}, ["rotation_euler"])
        _insert_from_user_preference_test({"SCALE"}, ["scale"])
        _insert_from_user_preference_test({"LOCATION", "ROTATION", "SCALE"}, ["location", "rotation_euler", "scale"])


class VisualKeyingTest(AbstractKeyframingTest, unittest.TestCase):
    """ Check if visual keying produces the correct keyframe values. """

    def tearDown(self):
        super().tearDown()
        bpy.context.preferences.edit.use_visual_keying = False

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


class CycleAwareKeyingTest(AbstractKeyframingTest, unittest.TestCase):
    """ Check if cycle aware keying remaps the keyframes correctly and adds fcurve modifiers. """

    def setUp(self):
        super().setUp()
        bpy.context.scene.tool_settings.use_keyframe_cycle_aware = True

    def test_insert_by_name(self):
        # In order to make cycle aware keying work, the action needs to be created and have the
        # frame_range set plus the use_frame_range flag set to True.
        keyed_object = _create_animation_object()

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert_by_name(type="Location")

        action = keyed_object.animation_data.action
        action.use_cyclic = True
        action.use_frame_range = True
        action.frame_range = [1, 20]

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(5)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")

            # Will be mapped to frame 3.
            bpy.context.preferences.edit.key_insert_channels = {"LOCATION"}
            bpy.context.scene.frame_set(22)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")

            # Will be mapped to frame 9.
            bpy.context.scene.frame_set(-10)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")

        # Check that only location keys have been created.
        _fcurve_paths_match(action.fcurves, ["location"])

        expected_keys = [1, 3, 5, 9, 20]

        for fcurve in action.fcurves:
            self.assertEqual(len(fcurve.keyframe_points), len(expected_keys))
            key_index = 0
            for key in fcurve.keyframe_points:
                self.assertEqual(key.co.x, expected_keys[key_index])
                key_index += 1

            # All fcurves should have a cycles modifier.
            self.assertTrue(fcurve.modifiers[0].type == "CYCLES")

    def test_insert_key(self):
        keyed_object = _create_animation_object()

        bpy.context.preferences.edit.key_insert_channels = {'LOCATION'}

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert()

        action = keyed_object.animation_data.action
        action.use_cyclic = True
        action.use_frame_range = True
        action.frame_range = [1, 20]

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(5)
            bpy.ops.anim.keyframe_insert()

            # Will be mapped to frame 3.
            bpy.context.scene.frame_set(22)
            bpy.ops.anim.keyframe_insert()

        expected_keys = [1, 3, 5, 20]

        for fcurve in action.fcurves:
            self.assertEqual(len(fcurve.keyframe_points), len(expected_keys))
            key_index = 0
            for key in fcurve.keyframe_points:
                self.assertEqual(key.co.x, expected_keys[key_index])
                key_index += 1

            # All fcurves should have a cycles modifier.
            self.assertTrue(fcurve.modifiers[0].type == "CYCLES")


class AutoKeyframingTest(AbstractKeyframingTest, unittest.TestCase):

    def setUp(self):
        super().setUp()
        bpy.context.scene.tool_settings.use_keyframe_insert_auto = True
        bpy.context.preferences.edit.use_keyframe_insert_available = False
        bpy.context.preferences.edit.use_keyframe_insert_needed = False
        bpy.context.preferences.edit.use_auto_keyframe_insert_needed = False

    def tearDown(self):
        super().tearDown()
        bpy.context.scene.tool_settings.use_keyframe_insert_auto = False

    def test_autokey_basic(self):
        keyed_object = _create_animation_object()
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.transform.translate(value=(1, 0, 0))

        action = keyed_object.animation_data.action
        _fcurve_paths_match(action.fcurves, ["location", "rotation_euler", "scale"])

    def test_autokey_bone(self):
        armature_obj = _create_armature()

        bpy.ops.object.mode_set(mode='POSE')
        # Not overriding the context because it would mean context.selected_pose_bones is empty
        # resulting in a failure to move/key the bone
        bpy.ops.transform.translate(value=(1, 0, 0))
        bpy.ops.object.mode_set(mode='OBJECT')

        action = armature_obj.animation_data.action
        bone_path = f"pose.bones[\"{_BONE_NAME}\"]"
        expected_paths = [f"{bone_path}.location", f"{bone_path}.rotation_euler", f"{bone_path}.scale"]
        _fcurve_paths_match(action.fcurves, expected_paths)


class InsertAvailableTest(AbstractKeyframingTest, unittest.TestCase):

    def setUp(self):
        super().setUp()
        bpy.context.scene.tool_settings.use_keyframe_insert_auto = True
        bpy.context.preferences.edit.use_keyframe_insert_available = True
        bpy.context.preferences.edit.use_keyframe_insert_needed = False
        bpy.context.preferences.edit.use_auto_keyframe_insert_needed = False

    def tearDown(self):
        super().tearDown()
        bpy.context.scene.tool_settings.use_keyframe_insert_auto = False
        bpy.context.preferences.edit.use_keyframe_insert_available = False

    def test_autokey_available_object(self):
        keyed_object = _create_animation_object()

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(1)
            bpy.ops.anim.keyframe_insert_by_name(type="Rotation")
            bpy.context.scene.frame_set(5)
            bpy.ops.transform.translate(value=(1, 0, 0))

        # Test that no new keyframes have been added.
        action = keyed_object.animation_data.action
        _fcurve_paths_match(action.fcurves, ["rotation_euler"])

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(1)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")
            bpy.context.scene.frame_set(5)
            bpy.ops.transform.translate(value=(1, 0, 0))

        action = keyed_object.animation_data.action
        _fcurve_paths_match(action.fcurves, ["location", "rotation_euler"])

        for fcurve in action.fcurves:
            # Translating the bone would also add rotation keys as long as "Only Insert Needed" is off.
            if "location" in fcurve.data_path or "rotation" in fcurve.data_path:
                self.assertEqual(len(fcurve.keyframe_points), 2)
            else:
                raise AssertionError(f"Did not expect keys other than location and rotation, got {fcurve.data_path}.")

    def test_autokey_available_bone(self):
        armature_obj = _create_armature()

        bpy.ops.object.mode_set(mode='POSE')
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(1)
            bpy.ops.anim.keyframe_insert_by_name(type="Rotation")
            bpy.context.scene.frame_set(5)
            bpy.ops.transform.translate(value=(1, 0, 0))

        # Test that no new keyframes have been added.
        action = armature_obj.animation_data.action
        bone_path = f"pose.bones[\"{_BONE_NAME}\"]"
        expected_paths = [f"{bone_path}.rotation_euler"]
        _fcurve_paths_match(action.fcurves, expected_paths)

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(1)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")
            bpy.context.scene.frame_set(5)
            bpy.ops.transform.translate(value=(1, 0, 0))

        expected_paths = [f"{bone_path}.location", f"{bone_path}.rotation_euler"]
        _fcurve_paths_match(action.fcurves, expected_paths)

        for fcurve in action.fcurves:
            # Translating the bone would also add rotation keys as long as "Only Insert Needed" is off.
            if "location" in fcurve.data_path or "rotation" in fcurve.data_path:
                self.assertEqual(len(fcurve.keyframe_points), 2)
            else:
                raise AssertionError(f"Did not expect keys other than location and rotation, got {fcurve.data_path}.")

    def test_insert_available_keying_set(self):
        keyed_object = _create_animation_object()

        with bpy.context.temp_override(**_get_view3d_context()):
            self.assertRaises(RuntimeError, bpy.ops.anim.keyframe_insert_by_name, type="Available")

        self.assertIsNone(keyed_object.animation_data)

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(1)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")
            bpy.context.scene.frame_set(5)
            bpy.ops.anim.keyframe_insert_by_name(type="Available")

        action = keyed_object.animation_data.action
        _fcurve_paths_match(action.fcurves, ["location"])

        for fcurve in action.fcurves:
            self.assertEqual(len(fcurve.keyframe_points), 2)


class InsertNeededTest(AbstractKeyframingTest, unittest.TestCase):

    def setUp(self):
        super().setUp()
        bpy.context.scene.tool_settings.use_keyframe_insert_auto = True
        bpy.context.preferences.edit.use_keyframe_insert_needed = True
        bpy.context.preferences.edit.use_auto_keyframe_insert_needed = True
        bpy.context.preferences.edit.use_keyframe_insert_available = False

    def tearDown(self):
        super().tearDown()
        bpy.context.scene.tool_settings.use_keyframe_insert_auto = False
        bpy.context.preferences.edit.use_keyframe_insert_needed = False
        bpy.context.preferences.edit.use_auto_keyframe_insert_needed = False

    def test_insert_needed_object(self):
        keyed_object = _create_animation_object()

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(1)
            bpy.ops.transform.translate(value=(-1, 0, 0))
            bpy.context.scene.frame_set(5)
            bpy.ops.transform.translate(value=(1, 0, 0))

        action = keyed_object.animation_data.action
        _fcurve_paths_match(action.fcurves, ["location"])

        # With "Insert Needed" enabled it has to key all location channels first,
        # before it can add keys only to the channels where values have actually
        # changed.
        expected_keys = {
            "location": (2, 1, 1)
        }

        self.assertEqual(len(action.fcurves), 3)

        for fcurve in action.fcurves:
            if fcurve.data_path not in expected_keys:
                raise AssertionError(f"Did not expect a key on {fcurve.data_path}")
            self.assertEqual(expected_keys[fcurve.data_path][fcurve.array_index], len(fcurve.keyframe_points))

    def test_insert_needed_bone(self):
        armature_obj = _create_armature()

        bpy.ops.object.mode_set(mode='POSE')
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(1)
            bpy.ops.transform.translate(value=(-1, 0, 0), orient_type='LOCAL')
            bpy.context.scene.frame_set(5)
            bpy.ops.transform.translate(value=(1, 0, 0), orient_type='LOCAL')

        bpy.ops.object.mode_set(mode='OBJECT')

        action = armature_obj.animation_data.action
        bone_path = f"pose.bones[\"{_BONE_NAME}\"]"
        _fcurve_paths_match(action.fcurves, [f"{bone_path}.location"])

        # With "Insert Needed" enabled it has to key all location channels first,
        # before it can add keys only to the channels where values have actually
        # changed.
        expected_keys = {
            f"{bone_path}.location": (2, 1, 1)
        }

        self.assertEqual(len(action.fcurves), 3)

        for fcurve in action.fcurves:
            if fcurve.data_path not in expected_keys:
                raise AssertionError(f"Did not expect a key on {fcurve.data_path}")
            self.assertEqual(expected_keys[fcurve.data_path][fcurve.array_index], len(fcurve.keyframe_points))


def _create_nla_anim_object():
    """
    Creates an object with 3 NLA tracks each with a strip that has its own action.
    The middle layer is additive.
    Creates a key on frame 0 and frame 10 for each of them.
    The values are:
        top: 0, 0
        add: 0, 1
        base: 0, 1
    """
    anim_object = bpy.data.objects.new("anim_object", None)
    bpy.context.scene.collection.objects.link(anim_object)
    bpy.context.view_layer.objects.active = anim_object
    anim_object.select_set(True)
    anim_object.animation_data_create()

    track = anim_object.animation_data.nla_tracks.new()
    track.name = "base"
    action_base = bpy.data.actions.new(name="action_base")
    fcu = action_base.fcurves.new(data_path="location", index=0)
    fcu.keyframe_points.insert(0, value=0).interpolation = 'LINEAR'
    fcu.keyframe_points.insert(10, value=1).interpolation = 'LINEAR'
    track.strips.new("base_strip", 0, action_base)

    track = anim_object.animation_data.nla_tracks.new()
    track.name = "add"
    action_add = bpy.data.actions.new(name="action_add")
    fcu = action_add.fcurves.new(data_path="location", index=0)
    fcu.keyframe_points.insert(0, value=0).interpolation = 'LINEAR'
    fcu.keyframe_points.insert(10, value=1).interpolation = 'LINEAR'
    strip = track.strips.new("add_strip", 0, action_add)
    strip.blend_type = "ADD"

    track = anim_object.animation_data.nla_tracks.new()
    track.name = "top"
    action_top = bpy.data.actions.new(name="action_top")
    fcu = action_top.fcurves.new(data_path="location", index=0)
    fcu.keyframe_points.insert(0, value=0).interpolation = 'LINEAR'
    fcu.keyframe_points.insert(10, value=0).interpolation = 'LINEAR'
    track.strips.new("top_strip", 0, action_top)

    return anim_object


class NlaInsertTest(AbstractKeyframingTest, unittest.TestCase):
    """
    Testing inserting keys into an NLA stack.
    The system is expected to remap the inserted values based on the strips blend_type.
    """

    def setUp(self):
        super().setUp()
        bpy.context.preferences.edit.key_insert_channels = {'LOCATION'}
        # Change one area to the NLA so we can call operators in it.
        # Assumes there is at least one editor in the blender default startup file that is not the 3D viewport.
        for area in bpy.context.window.screen.areas:
            if area.type == 'VIEW_3D':
                continue
            area.type = "NLA_EDITOR"
            break

    def test_insert_failure(self):
        # If the topmost track is set to "REPLACE" the system will fail
        # when trying to insert keys into a layer beneath.
        nla_anim_object = _create_nla_anim_object()
        tracks = nla_anim_object.animation_data.nla_tracks

        with bpy.context.temp_override(**_get_nla_context()):
            bpy.ops.nla.select_all(action="DESELECT")
            tracks.active = tracks["base"]
            tracks["base"].strips[0].select = True
            bpy.ops.nla.tweakmode_enter(use_upper_stack_evaluation=True)

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(5)
            bpy.ops.anim.keyframe_insert()

        base_action = bpy.data.actions["action_base"]
        # Location X should not have been able to insert a keyframe because the top strip is overriding the result completely,
        # making it impossible to calculate which value should be inserted.
        self.assertEqual(len(base_action.fcurves.find("location", index=0).keyframe_points), 2)
        # Location Y and Z will go through since they have not been defined in the action of the top strip.
        self.assertEqual(len(base_action.fcurves.find("location", index=1).keyframe_points), 1)
        self.assertEqual(len(base_action.fcurves.find("location", index=2).keyframe_points), 1)

    def test_insert_additive(self):
        nla_anim_object = _create_nla_anim_object()
        tracks = nla_anim_object.animation_data.nla_tracks

        # This leaves the additive track as the topmost track with influence
        tracks["top"].mute = True

        with bpy.context.temp_override(**_get_nla_context()):
            bpy.ops.nla.select_all(action="DESELECT")
            tracks.active = tracks["base"]
            tracks["base"].strips[0].select = True
            bpy.ops.nla.tweakmode_enter(use_upper_stack_evaluation=True)

        # Inserting over the existing keyframe.
        bpy.context.scene.frame_set(10)
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert()

        base_action = bpy.data.actions["action_base"]
        # This should have added keys to Y and Z but not X.
        # X already had two keys from the file setup.
        self.assertEqual(len(base_action.fcurves.find("location", index=0).keyframe_points), 2)
        self.assertEqual(len(base_action.fcurves.find("location", index=1).keyframe_points), 1)
        self.assertEqual(len(base_action.fcurves.find("location", index=2).keyframe_points), 1)

        # The keyframe value should not be changed even though the position of the
        # object is modified by the additive layer.
        self.assertAlmostEqual(nla_anim_object.location.x, 2.0, 8)
        fcurve_loc_x = base_action.fcurves.find("location", index=0)
        self.assertAlmostEqual(fcurve_loc_x.keyframe_points[-1].co[1], 1.0, 8)


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
