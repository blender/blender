# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import bpy
import pathlib
import sys
from math import radians

"""
blender -b --factory-startup --python tests/python/bl_animation_keyframing.py -- --testdir /path/to/tests/files/animation
"""


def _fcurve_paths_match(fcurves: list[bpy.types.FCurve], expected_paths: list[str]) -> None:
    data_paths = list(set([fcurve.data_path for fcurve in fcurves]))
    data_paths.sort()
    expected_paths.sort()
    if data_paths != expected_paths:
        raise AssertionError(
            f"Expected paths do not match F-Curve paths. Expected: {expected_paths}. F-Curve: {data_paths}")


def _first_channelbag(action: bpy.types.Action) -> bpy.types.ActionChannelbag:
    """Return the first Channelbag of the Action."""
    assert isinstance(action, bpy.types.Action), f"Expected Action, got {action!r}"
    return action.layers[0].strips[0].channelbags[0]


def _channelbag(animated_id: bpy.types.ID) -> bpy.types.ActionChannelbag:
    """Return the first layer's Channelbag of the animated ID's Action."""
    action = animated_id.animation_data.action
    action_slot = animated_id.animation_data.action_slot
    channelbag = action.layers[0].strips[0].channelbag(action_slot)
    assert channelbag is not None
    return channelbag


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
    armature_obj.pose.bones[_BONE_NAME].select = True
    bpy.ops.object.mode_set(mode='OBJECT')

    return armature_obj


def _insert_by_name_test(insert_key: str, expected_paths: list):
    keyed_object = _create_animation_object()
    with bpy.context.temp_override(**_get_view3d_context()):
        bpy.ops.anim.keyframe_insert_by_name(type=insert_key)
    _fcurve_paths_match(_channelbag(keyed_object).fcurves, expected_paths)
    bpy.data.objects.remove(keyed_object, do_unlink=True)


def _insert_from_user_preference_test(enabled_user_pref_fields: set, expected_paths: list):
    keyed_object = _create_animation_object()
    bpy.context.preferences.edit.key_insert_channels = enabled_user_pref_fields
    with bpy.context.temp_override(**_get_view3d_context()):
        bpy.ops.anim.keyframe_insert()
    _fcurve_paths_match(_channelbag(keyed_object).fcurves, expected_paths)
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
    _fcurve_paths_match(_channelbag(keyed_object).fcurves, expected_paths)
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

    def test_keying_creates_default_groups(self):
        keyed_object = _create_animation_object()

        bpy.context.preferences.edit.key_insert_channels = {'LOCATION'}
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert()

        channelbag = _channelbag(keyed_object)

        # Check the F-Curves paths.
        expect_paths = ["location", "location", "location"]
        actual_paths = [fcurve.data_path for fcurve in channelbag.fcurves]
        self.assertEqual(actual_paths, expect_paths)

        # The actual reason for this test: check that these curves have the right group.
        expect_groups = ["Object Transforms"]
        actual_groups = [group.name for group in channelbag.groups]
        self.assertEqual(actual_groups, expect_groups)

        expect_groups = 3 * [channelbag.groups[0]]
        actual_groups = [fcurve.group for fcurve in channelbag.fcurves]
        self.assertEqual(actual_groups, expect_groups)

    def test_insert_custom_properties(self):
        # Used to create a datablock reference property.
        ref_object = bpy.data.objects.new("ref_object", None)
        bpy.context.scene.collection.objects.link(ref_object)

        bpy.context.preferences.edit.key_insert_channels = {"CUSTOM_PROPS"}
        keyed_object = _create_animation_object()

        keyed_properties = {
            "int": 1,
            "float": 1.0,
            "bool": True,
            "int_array": [1, 2, 3],
            "float_array": [1.0, 2.0, 3.0],
            "bool_array": [True, False, True],
            "'escaped'": 1,
            '"escaped"': 1
        }

        unkeyed_properties = {
            "str": "unkeyed",
            "reference": ref_object,
        }

        for path, value in keyed_properties.items():
            keyed_object[path] = value

        for path, value in unkeyed_properties.items():
            keyed_object[path] = value

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert()

        keyed_rna_paths = [f"[\"{bpy.utils.escape_identifier(path)}\"]" for path in keyed_properties.keys()]
        _fcurve_paths_match(_channelbag(keyed_object).fcurves, keyed_rna_paths)
        bpy.data.objects.remove(keyed_object, do_unlink=True)

    def test_key_selection_state(self):
        keyed_object = _create_animation_object()
        bpy.context.preferences.edit.key_insert_channels = {"LOCATION"}
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert()
            bpy.context.scene.frame_set(5)
            bpy.ops.anim.keyframe_insert()

        for fcurve in _channelbag(keyed_object).fcurves:
            self.assertEqual(len(fcurve.keyframe_points), 2)
            self.assertFalse(fcurve.keyframe_points[0].select_control_point)
            self.assertTrue(fcurve.keyframe_points[1].select_control_point)

    def test_keyframe_insert_py_func(self):
        curve_object = _create_animation_object()

        # Test on location, which is a 3-item array, without explicitly passing an array index.
        self.assertTrue(curve_object.keyframe_insert('location'))

        ob_fcurves = _channelbag(curve_object).fcurves

        self.assertEqual(len(ob_fcurves), 3,
                         "Keying 'location' without any array index should have created 3 F-Curves")
        self.assertEqual(3 * ['location'], [fcurve.data_path for fcurve in ob_fcurves])
        self.assertEqual([0, 1, 2], [fcurve.array_index for fcurve in ob_fcurves])

        ob_fcurves.clear()

        # Test on 'rotation_quaterion' (4 items), with explicit index=-1.
        self.assertTrue(curve_object.keyframe_insert('rotation_quaternion', index=-1))
        self.assertEqual(len(ob_fcurves), 4,
                         "Keying 'rotation_quaternion' with index=-1 should have created 4 F-Curves")
        self.assertEqual(4 * ['rotation_quaternion'], [fcurve.data_path for fcurve in ob_fcurves])
        self.assertEqual([0, 1, 2, 3], [fcurve.array_index for fcurve in ob_fcurves])

        ob_fcurves.clear()

        # Test on 'scale' (3 items) with explicit index=1.
        self.assertTrue(curve_object.keyframe_insert('scale', index=2))
        self.assertEqual(len(ob_fcurves), 1,
                         "Keying 'scale' with index=2 should have created 1 F-Curve")
        self.assertEqual('scale', ob_fcurves[0].data_path)
        self.assertEqual(2, ob_fcurves[0].array_index)

    def test_keyframe_insert_py_func_with_group(self):
        curve_object = _create_animation_object()

        # Test with property for which Blender knows a group name too ('Object Transforms').
        self.assertTrue(curve_object.keyframe_insert('location', group="Téšt"))

        channelbag = _channelbag(curve_object)
        fcurves = channelbag.fcurves
        fgroups = channelbag.groups

        self.assertEqual(3 * ['location'], [fcurve.data_path for fcurve in fcurves])
        self.assertEqual([0, 1, 2], [fcurve.array_index for fcurve in fcurves])
        self.assertEqual(["Téšt"], [group.name for group in fgroups])
        self.assertEqual(3 * ["Téšt"], [fcurve.group and fcurve.group.name for fcurve in fcurves])

        fcurves.clear()
        while fgroups:
            fgroups.remove(fgroups[0])

        # Test with property that does not have predefined group name.
        self.assertTrue(curve_object.keyframe_insert('show_wire', group="Téšt"))
        self.assertEqual('show_wire', fcurves[0].data_path)
        self.assertEqual(["Téšt"], [group.name for group in fgroups])

    def test_keyframe_insert_nested_rna_path(self):
        bpy.ops.mesh.primitive_cube_add()
        obj = bpy.context.object
        obj.data.attributes.new("test", "FLOAT", "POINT")
        self.assertTrue(obj.data.keyframe_insert('attributes["test"].data[0].value'))
        fcurves = _channelbag(obj.data).fcurves
        self.assertEqual(len(fcurves), 1)
        self.assertEqual(fcurves[0].data_path, 'attributes["test"].data[0].value')


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

        for fcurve in _channelbag(constrained).fcurves:
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

        for fcurve in _channelbag(constrained).fcurves:
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

        for fcurve in _channelbag(constrained).fcurves:
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

        for fcurve in _channelbag(constrained).fcurves:
            self.assertEqual(fcurve.keyframe_points[0].co.y, t_value)


class CycleAwareKeyingTest(AbstractKeyframingTest, unittest.TestCase):
    """ Check if cycle aware keying remaps the keyframes correctly and adds fcurve modifiers. """

    def setUp(self):
        super().setUp()
        bpy.context.scene.tool_settings.use_keyframe_cycle_aware = True

        # Deselect the default cube, because this test works on a specific
        # object. Operators that work on all selected objects shouldn't work on
        # anything else but that object.
        bpy.ops.object.select_all(action='DESELECT')

    def tearDown(self):
        bpy.context.scene.tool_settings.use_keyframe_cycle_aware = False
        super().tearDown()

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
        channelbag = action.layers[0].strips[0].channelbags[0]
        _fcurve_paths_match(channelbag.fcurves, ["location"])

        expected_keys = [1.0, 3.0, 5.0, 9.0, 20.0]

        for fcurve in channelbag.fcurves:
            actual_keys = [key.co.x for key in fcurve.keyframe_points]
            self.assertEqual(expected_keys, actual_keys)

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

        expected_keys = [1.0, 3.0, 5.0, 20.0]

        channelbag = action.layers[0].strips[0].channelbags[0]
        for fcurve in channelbag.fcurves:
            actual_keys = [key.co.x for key in fcurve.keyframe_points]
            self.assertEqual(expected_keys, actual_keys)

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

        channelbag = _channelbag(keyed_object)
        _fcurve_paths_match(channelbag.fcurves, ["location", "rotation_euler", "scale"])

    def test_autokey_bone(self):
        armature_obj = _create_armature()

        bpy.ops.object.mode_set(mode='POSE')
        # Not overriding the context because it would mean context.selected_pose_bones is empty
        # resulting in a failure to move/key the bone
        bpy.ops.transform.translate(value=(1, 0, 0))
        bpy.ops.object.mode_set(mode='OBJECT')

        channelbag = _channelbag(armature_obj)
        bone_path = f"pose.bones[\"{_BONE_NAME}\"]"
        expected_paths = [f"{bone_path}.location", f"{bone_path}.rotation_euler", f"{bone_path}.scale"]
        _fcurve_paths_match(channelbag.fcurves, expected_paths)

    def test_key_selection_state(self):
        armature_obj = _create_armature()
        bpy.ops.object.mode_set(mode='POSE')
        bpy.ops.transform.translate(value=(1, 0, 0))
        bpy.context.scene.frame_set(5)
        bpy.ops.transform.translate(value=(0, 1, 0))
        bpy.ops.object.mode_set(mode='OBJECT')

        channelbag = _channelbag(armature_obj)
        for fcurve in channelbag.fcurves:
            self.assertEqual(len(fcurve.keyframe_points), 2)
            self.assertFalse(fcurve.keyframe_points[0].select_control_point)
            self.assertTrue(fcurve.keyframe_points[1].select_control_point)


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
        channelbag = _channelbag(keyed_object)
        _fcurve_paths_match(channelbag.fcurves, ["rotation_euler"])

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(1)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")
            bpy.context.scene.frame_set(5)
            bpy.ops.transform.translate(value=(1, 0, 0))

        channelbag = _channelbag(keyed_object)
        _fcurve_paths_match(channelbag.fcurves, ["location", "rotation_euler"])

        for fcurve in channelbag.fcurves:
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
        channelbag = _channelbag(armature_obj)
        bone_path = f"pose.bones[\"{_BONE_NAME}\"]"
        expected_paths = [f"{bone_path}.rotation_euler"]
        _fcurve_paths_match(channelbag.fcurves, expected_paths)

        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.context.scene.frame_set(1)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")
            bpy.context.scene.frame_set(5)
            bpy.ops.transform.translate(value=(1, 0, 0))

        expected_paths = [f"{bone_path}.location", f"{bone_path}.rotation_euler"]
        _fcurve_paths_match(channelbag.fcurves, expected_paths)

        for fcurve in channelbag.fcurves:
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

        channelbag = _channelbag(keyed_object)
        _fcurve_paths_match(channelbag.fcurves, ["location"])

        for fcurve in channelbag.fcurves:
            self.assertEqual(len(fcurve.keyframe_points), 2)

    def test_insert_available(self):
        keyed_object = _create_animation_object()
        self.assertIsNone(keyed_object.animation_data, "Precondition check: test object should not have animdata yet")

        keyed_ok = keyed_object.keyframe_insert("location", options={'INSERTKEY_AVAILABLE'})
        self.assertFalse(keyed_ok, "Should not key with INSERTKEY_AVAILABLE when no F-Curves are available")


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

        channelbag = _channelbag(keyed_object)
        _fcurve_paths_match(channelbag.fcurves, ["location"])

        # With "Insert Needed" enabled it has to key all location channels first,
        # before it can add keys only to the channels where values have actually
        # changed.
        expected_keys = {
            "location": (2, 1, 1)
        }

        self.assertEqual(len(channelbag.fcurves), 3)

        for fcurve in channelbag.fcurves:
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

        channelbag = _channelbag(armature_obj)
        bone_path = f"pose.bones[\"{_BONE_NAME}\"]"
        _fcurve_paths_match(channelbag.fcurves, [f"{bone_path}.location"])

        # With "Insert Needed" enabled it has to key all location channels first,
        # before it can add keys only to the channels where values have actually
        # changed.
        expected_keys = {
            f"{bone_path}.location": (2, 1, 1)
        }

        self.assertEqual(len(channelbag.fcurves), 3)

        for fcurve in channelbag.fcurves:
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

    def _ensure_fcurve(action: bpy.types.Action, *, data_path: str, index: int) -> bpy.types.FCurve:
        # Briefly directly assign the Action so that Blender knows what to do.
        anim_object.animation_data_create().action = action
        try:
            fcurve = action.fcurve_ensure_for_datablock(anim_object, data_path=data_path, index=index)
        finally:
            anim_object.animation_data.action = None

        return fcurve

    bpy.context.scene.collection.objects.link(anim_object)
    bpy.context.view_layer.objects.active = anim_object
    anim_object.select_set(True)
    anim_object.animation_data_create()

    track = anim_object.animation_data.nla_tracks.new()
    track.name = "base"
    action_base = bpy.data.actions.new(name="action_base")
    fcu = _ensure_fcurve(action_base, data_path="location", index=0)
    fcu.keyframe_points.insert(0, value=0).interpolation = 'LINEAR'
    fcu.keyframe_points.insert(10, value=1).interpolation = 'LINEAR'
    track.strips.new("base_strip", 0, action_base)
    assert action_base.is_action_layered

    track = anim_object.animation_data.nla_tracks.new()
    track.name = "add"
    action_add = bpy.data.actions.new(name="action_add")
    fcu = _ensure_fcurve(action_add, data_path="location", index=0)
    fcu.keyframe_points.insert(0, value=0).interpolation = 'LINEAR'
    fcu.keyframe_points.insert(10, value=1).interpolation = 'LINEAR'
    strip = track.strips.new("add_strip", 0, action_add)
    strip.blend_type = "ADD"
    assert action_add.is_action_layered

    track = anim_object.animation_data.nla_tracks.new()
    track.name = "top"
    action_top = bpy.data.actions.new(name="action_top")
    fcu = _ensure_fcurve(action_top, data_path="location", index=0)
    fcu.keyframe_points.insert(0, value=0).interpolation = 'LINEAR'
    fcu.keyframe_points.insert(10, value=0).interpolation = 'LINEAR'
    track.strips.new("top_strip", 0, action_top)
    assert action_top.is_action_layered

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

        # Deselect the default cube, because the NLA tests work on a specific
        # object created for that test. Operators that work on all selected
        # objects shouldn't work on anything else but that object.
        bpy.ops.object.select_all(action='DESELECT')

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
        channelbag = _first_channelbag(base_action)
        # Location X should not have been able to insert a keyframe because the top strip is overriding the result completely,
        # making it impossible to calculate which value should be inserted.
        self.assertEqual(len(channelbag.fcurves.find("location", index=0).keyframe_points), 2)
        # Location Y and Z will go through since they have not been defined in the action of the top strip.
        self.assertEqual(len(channelbag.fcurves.find("location", index=1).keyframe_points), 1)
        self.assertEqual(len(channelbag.fcurves.find("location", index=2).keyframe_points), 1)

    def test_insert_additive(self):
        nla_anim_object = _create_nla_anim_object()
        tracks = nla_anim_object.animation_data.nla_tracks

        self.assertEqual(nla_anim_object, bpy.context.active_object)
        self.assertEqual(None, nla_anim_object.animation_data.action)

        # This leaves the additive track as the topmost track with influence
        tracks["top"].mute = True

        with bpy.context.temp_override(**_get_nla_context()):
            bpy.ops.nla.select_all(action="DESELECT")
            tracks.active = tracks["base"]
            tracks["base"].strips[0].select = True
            bpy.ops.nla.tweakmode_enter(use_upper_stack_evaluation=True)

        base_action = bpy.data.actions["action_base"]

        # Verify that tweak mode has switched to the correct Action.
        self.assertEqual(base_action, nla_anim_object.animation_data.action)

        # Inserting over the existing keyframe.
        bpy.context.scene.frame_set(10)
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert()

        # Check that the expected F-Curves exist.
        channelbag = _first_channelbag(base_action)
        fcurves_actual = {(f.data_path, f.array_index) for f in channelbag.fcurves}
        fcurves_expect = {
            ("location", 0),
            ("location", 1),
            ("location", 2),
        }
        self.assertEqual(fcurves_actual, fcurves_expect)

        # This should have added keys to Y and Z but not X.
        # X already had two keys from the file setup.
        self.assertEqual(len(channelbag.fcurves.find("location", index=0).keyframe_points), 2)
        self.assertEqual(len(channelbag.fcurves.find("location", index=1).keyframe_points), 1)
        self.assertEqual(len(channelbag.fcurves.find("location", index=2).keyframe_points), 1)

        # The keyframe value should not be changed even though the position of the
        # object is modified by the additive layer.
        self.assertAlmostEqual(nla_anim_object.location.x, 2.0, 8)
        fcurve_loc_x = channelbag.fcurves.find("location", index=0)
        self.assertAlmostEqual(fcurve_loc_x.keyframe_points[-1].co[1], 1.0, 8)


class KeyframeDeleteTest(AbstractKeyframingTest, unittest.TestCase):

    def test_delete_in_v3d_pose_mode(self):
        armature = _create_armature()
        bpy.context.scene.frame_set(1)
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert_by_name(type="Location")
        self.assertTrue(armature.animation_data is not None)
        self.assertTrue(armature.animation_data.action is not None)
        channelbag = _channelbag(armature)
        self.assertEqual(len(channelbag.fcurves), 3)

        bpy.ops.object.mode_set(mode='POSE')
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_insert_by_name(type="Location")
            bpy.context.scene.frame_set(5)
            bpy.ops.anim.keyframe_insert_by_name(type="Location")
            # This should have added new FCurves for the pose bone.
            self.assertEqual(len(channelbag.fcurves), 6)

            bpy.ops.anim.keyframe_delete_v3d()
            # No Fcurves should yet be deleted.
            self.assertEqual(len(channelbag.fcurves), 6)
            self.assertEqual(len(channelbag.fcurves[0].keyframe_points), 1)
            bpy.context.scene.frame_set(1)
            bpy.ops.anim.keyframe_delete_v3d()
            # This should leave the object level keyframes of the armature
            self.assertEqual(len(channelbag.fcurves), 3)

        bpy.ops.object.mode_set(mode='OBJECT')
        with bpy.context.temp_override(**_get_view3d_context()):
            bpy.ops.anim.keyframe_delete_v3d()
        # The last FCurves should be deleted from the object now.
        self.assertEqual(len(channelbag.fcurves), 0)


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
