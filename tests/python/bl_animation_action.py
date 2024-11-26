# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import sys
import pathlib

import bpy

"""
blender -b --factory-startup --python tests/python/bl_animation_action.py
"""


class ActionSlotAssignmentTest(unittest.TestCase):
    """Test assigning actions & check reference counts."""

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)

    def test_action_assignment(self):
        # Create new Action.
        action = bpy.data.actions.new('TestAction')
        self.assertEqual(0, action.users)

        # Assign the animation to the cube,
        cube = bpy.data.objects['Cube']
        cube_adt = cube.animation_data_create()
        cube_adt.action = action
        self.assertEqual(1, action.users)

        # Assign the animation to the camera as well.
        camera = bpy.data.objects['Camera']
        camera_adt = camera.animation_data_create()
        camera_adt.action = action
        self.assertEqual(2, action.users)

        # Unassigning should decrement the user count.
        cube_adt.action = None
        self.assertEqual(1, action.users)

        # Deleting the camera should also decrement the user count.
        bpy.data.objects.remove(camera)
        self.assertEqual(0, action.users)

    def test_slot_assignment(self):
        # Create new Action.
        action = bpy.data.actions.new('TestAction')
        self.assertEqual(0, action.users)

        # Assign the Action to the cube,
        cube = bpy.data.objects['Cube']
        cube_adt = cube.animation_data_create()
        cube_adt.action = action
        slot_cube = action.slots.new(for_id=cube)
        cube_adt.action_slot_handle = slot_cube.handle
        self.assertEqual(cube_adt.action_slot_handle, slot_cube.handle)

        # Assign the Action to the camera as well.
        camera = bpy.data.objects['Camera']
        slot_camera = action.slots.new(for_id=camera)
        camera_adt = camera.animation_data_create()
        camera_adt.action = action
        self.assertEqual(camera_adt.action_slot_handle, slot_camera.handle)

        # Unassigning should keep the slot identifier.
        cube_adt.action = None
        self.assertEqual(cube_adt.action_slot_name, slot_cube.identifier)

        # It should not be possible to set the slot handle while the Action is unassigned.
        slot_extra = action.slots.new()
        cube_adt.action_slot_handle = slot_extra.handle
        self.assertNotEqual(cube_adt.action_slot_handle, slot_extra.handle)

        # Slots from another Action should be gracefully rejected.
        other_action = bpy.data.actions.new("That Other Action")
        slot = other_action.slots.new()
        cube_adt.action = action
        cube_adt.action_slot = slot_cube
        with self.assertRaises(RuntimeError):
            cube_adt.action_slot = slot
        self.assertEqual(cube_adt.action_slot, slot_cube, "The slot should not have changed")


class LimitationsTest(unittest.TestCase):
    """Test artificial limitations for the layered Action.

    Certain limitations are in place to keep development & testing focused.
    """

    def setUp(self):
        anims = bpy.data.actions
        while anims:
            anims.remove(anims[0])

    def test_initial_layers(self):
        """Test that upon creation an Action has no layers/strips."""
        action = bpy.data.actions.new('TestAction')
        self.assertEqual([], action.layers[:])

    def test_limited_layers_strips(self):
        """Test that there can only be one layer with one strip."""

        action = bpy.data.actions.new('TestAction')
        layer = action.layers.new(name="Layer")
        self.assertEqual([], layer.strips[:])
        strip = layer.strips.new(type='KEYFRAME')

        # Adding a 2nd layer should be forbidden.
        with self.assertRaises(RuntimeError):
            action.layers.new(name="Forbidden Layer")
        self.assertEqual([layer], action.layers[:])

        # Adding a 2nd strip should be forbidden.
        with self.assertRaises(RuntimeError):
            layer.strips.new(type='KEYFRAME')
        self.assertEqual([strip], layer.strips[:])

    def test_limited_strip_api(self):
        """Test that strips have no frame start/end/offset properties."""

        action = bpy.data.actions.new('TestAction')
        layer = action.layers.new(name="Layer")
        strip = layer.strips.new(type='KEYFRAME')

        self.assertFalse(hasattr(strip, 'frame_start'))
        self.assertFalse(hasattr(strip, 'frame_end'))
        self.assertFalse(hasattr(strip, 'frame_offset'))


class LegacyAPIOnLayeredActionTest(unittest.TestCase):
    """Test that the legacy Action API works on layered Actions.

    It should give access to the keyframes for the first slot.

    - curve_frame_range
    - fcurves
    - groups
    - id_root (should always be 0 for layered Actions)
    - flip_with_pose(object)
    """

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)

        self.action = bpy.data.actions.new('LayeredAction')

    def test_fcurves_on_layered_action(self) -> None:
        slot = self.action.slots.new(for_id=bpy.data.objects['Cube'])

        layer = self.action.layers.new(name="Layer")
        strip = layer.strips.new(type='KEYFRAME')
        channelbag = strip.channelbags.new(slot=slot)

        # Create new F-Curves via legacy API, they should be stored on the ChannelBag.
        fcurve1 = self.action.fcurves.new("scale", index=1)
        fcurve2 = self.action.fcurves.new("scale", index=2)
        self.assertEqual([fcurve1, fcurve2], channelbag.fcurves[:], "Expected two F-Curves after creating them")
        self.assertEqual([fcurve1, fcurve2], self.action.fcurves[:],
                         "Expected the same F-Curves on the legacy API")

        # Find an F-Curve.
        self.assertEqual(fcurve2, self.action.fcurves.find("scale", index=2))

        # Create an already-existing F-Curve.
        try:
            self.action.fcurves.new("scale", index=2)
        except RuntimeError as ex:
            self.assertIn("F-Curve 'scale[2]' already exists in action 'LayeredAction'", str(ex))
        else:
            self.fail("expected RuntimeError not thrown")
        self.assertEqual([fcurve1, fcurve2], channelbag.fcurves[:],
                         "Expected two F-Curves after failing to create a third")
        self.assertEqual([fcurve1, fcurve2], self.action.fcurves[:])

        # Remove a single F-Curve.
        self.action.fcurves.remove(fcurve1)
        self.assertEqual([fcurve2], channelbag.fcurves[:], "Expected single F-Curve after removing one")
        self.assertEqual([fcurve2], self.action.fcurves[:])

        # Clear all F-Curves (with multiple F-Curves to avoid the trivial case).
        self.action.fcurves.new("scale", index=3)
        self.action.fcurves.clear()
        self.assertEqual([], channelbag.fcurves[:], "Expected empty fcurves list after clearing")
        self.assertEqual([], self.action.fcurves[:])

    def test_fcurves_clear_should_not_create_layers(self):
        self.action.fcurves.clear()
        self.assertEqual([], self.action.slots[:])
        self.assertEqual([], self.action.layers[:])

    def test_fcurves_new_on_empty_action(self) -> None:
        # Create new F-Curves via legacy API, this should create a layer+strip+ChannelBag.
        fcurve1 = self.action.fcurves.new("scale", index=1)
        fcurve2 = self.action.fcurves.new("scale", index=2)

        self.assertEqual(1, len(self.action.slots))
        self.assertEqual(1, len(self.action.layers))

        slot = self.action.slots[0]
        layer = self.action.layers[0]

        self.assertEqual(1, len(layer.strips))
        strip = layer.strips[0]
        self.assertEqual('KEYFRAME', strip.type)
        self.assertEqual(1, len(strip.channelbags))
        channelbag = strip.channelbags[0]
        self.assertEqual(channelbag.slot_handle, slot.handle)

        self.assertEqual([fcurve1, fcurve2], channelbag.fcurves[:])

        # After this, there is no need to test the rest of the functions, as the
        # Action will be in the same state as in test_fcurves_on_layered_action().

    def test_groups(self) -> None:
        # Create a group by using the legacy API to create an F-Curve with group name.
        group_name = "Object Transfoibles"
        self.action.fcurves.new("scale", index=1, action_group=group_name)

        layer = self.action.layers[0]
        strip = layer.strips[0]
        channelbag = strip.channelbags[0]

        self.assertEqual(1, len(channelbag.groups), "The new group should be available on the channelbag")
        self.assertEqual(group_name, channelbag.groups[0].name)
        self.assertEqual(1, len(self.action.groups), "The new group should be available with the legacy group API")
        self.assertEqual(group_name, self.action.groups[0].name)

        # Create a group via the legacy API.
        group = self.action.groups.new(group_name)
        self.assertEqual("{}.001".format(group_name), group.name, "The group should have a unique name")
        self.assertEqual(group, self.action.groups[1], "The group should be accessible via the legacy API")
        self.assertEqual(group, channelbag.groups[1], "The group should be accessible via the channelbag")

        # Remove a group via the legacy API.
        self.action.groups.remove(group)
        self.assertNotIn(group, self.action.groups[:], "A group should be removable via the legacy API")
        self.assertNotIn(group, channelbag.groups[:], "A group should be removable via the legacy API")


class ChannelBagsTest(unittest.TestCase):
    def setUp(self):
        anims = bpy.data.actions
        while anims:
            anims.remove(anims[0])

        self.action = bpy.data.actions.new('TestAction')

        self.slot = self.action.slots.new()
        self.slot.identifier = 'OBTest'

        self.layer = self.action.layers.new(name="Layer")
        self.strip = self.layer.strips.new(type='KEYFRAME')

    def test_create_remove_channelbag(self):
        channelbag = self.strip.channelbags.new(self.slot)

        self.strip.key_insert(self.slot, "location", 1, 47.0, 327.0)
        self.assertEqual("location", channelbag.fcurves[0].data_path,
                         "Keys for the channelbag's slot should go into the channelbag")

        self.strip.channelbags.remove(channelbag)
        self.assertEqual([], list(self.strip.channelbags))

    def test_create_remove_fcurves(self):
        channelbag = self.strip.channelbags.new(self.slot)

        # Creating an F-Curve should work.
        fcurve = channelbag.fcurves.new('location', index=1)
        self.assertIsNotNone(fcurve)
        self.assertEquals(fcurve.data_path, 'location')
        self.assertEquals(fcurve.array_index, 1)
        self.assertEquals([fcurve], channelbag.fcurves[:])

        # Empty data paths should not be accepted.
        with self.assertRaises(RuntimeError):
            channelbag.fcurves.new('', index=1)
        self.assertEquals([fcurve], channelbag.fcurves[:])

        # Creating an F-Curve twice should fail:
        with self.assertRaises(RuntimeError):
            channelbag.fcurves.new('location', index=1)
        self.assertEquals([fcurve], channelbag.fcurves[:])

        # Removing an unrelated F-Curve should fail, even when an F-Curve with
        # the same RNA path and array index exists.
        other_slot = self.action.slots.new()
        other_cbag = self.strip.channelbags.new(other_slot)
        other_fcurve = other_cbag.fcurves.new('location', index=1)
        with self.assertRaises(RuntimeError):
            channelbag.fcurves.remove(other_fcurve)
        self.assertEquals([fcurve], channelbag.fcurves[:])

        # Removing an existing F-Curve should work:
        channelbag.fcurves.remove(fcurve)
        self.assertEquals([], channelbag.fcurves[:])

    def test_fcurves_clear(self):
        channelbag = self.strip.channelbags.new(self.slot)

        for index in range(4):
            channelbag.fcurves.new('rotation_quaternion', index=index)

        self.assertEquals(4, len(channelbag.fcurves))
        channelbag.fcurves.clear()
        self.assertEquals([], channelbag.fcurves[:])

    def test_channel_groups(self):
        channelbag = self.strip.channelbags.new(self.slot)

        # Create some fcurves to play with.
        fcurve0 = channelbag.fcurves.new('location', index=0)
        fcurve1 = channelbag.fcurves.new('location', index=1)
        fcurve2 = channelbag.fcurves.new('location', index=2)
        fcurve3 = channelbag.fcurves.new('scale', index=0)
        fcurve4 = channelbag.fcurves.new('scale', index=1)
        fcurve5 = channelbag.fcurves.new('scale', index=2)

        self.assertEquals([], channelbag.groups[:])

        # Create some channel groups.
        group0 = channelbag.groups.new('group0')
        group1 = channelbag.groups.new('group1')
        self.assertEquals([group0, group1], channelbag.groups[:])
        self.assertEquals([], group0.channels[:])
        self.assertEquals([], group1.channels[:])

        # Assign some fcurves to the channel groups. Intentionally not in order
        # so we can test that the fcurves get moved around properly.
        fcurve5.group = group1
        fcurve3.group = group1
        fcurve2.group = group0
        fcurve4.group = group0
        self.assertEquals([fcurve2, fcurve4], group0.channels[:])
        self.assertEquals([fcurve5, fcurve3], group1.channels[:])
        self.assertEquals([fcurve2, fcurve4, fcurve5, fcurve3, fcurve0, fcurve1], channelbag.fcurves[:])

        # Weird case to be consistent with the legacy API: assigning None to an
        # fcurve's group does *not* unassign it from its group. This is stupid,
        # and we should change it at some point.  But it's how the legacy API
        # already works (presumably an oversight), so sticking to that for now.
        fcurve3.group = None
        self.assertEquals(group1, fcurve3.group)
        self.assertEquals([fcurve2, fcurve4], group0.channels[:])
        self.assertEquals([fcurve5, fcurve3], group1.channels[:])
        self.assertEquals([fcurve2, fcurve4, fcurve5, fcurve3, fcurve0, fcurve1], channelbag.fcurves[:])

        # Removing a group.
        channelbag.groups.remove(group0)
        self.assertEquals([group1], channelbag.groups[:])
        self.assertEquals([fcurve5, fcurve3], group1.channels[:])
        self.assertEquals([fcurve5, fcurve3, fcurve2, fcurve4, fcurve0, fcurve1], channelbag.fcurves[:])

        # Attempting to remove a channel group that belongs to a different
        # channel bag should fail.
        other_slot = self.action.slots.new()
        other_cbag = self.strip.channelbags.new(other_slot)
        other_group = other_cbag.groups.new('group1')
        with self.assertRaises(RuntimeError):
            channelbag.groups.remove(other_group)

        # Another weird case that we reproduce from the legacy API: attempting
        # to assign a group to an fcurve that doesn't belong to the same channel
        # bag should silently fail (just does a printf to stdout).
        fcurve0.group = other_group
        self.assertEquals([group1], channelbag.groups[:])
        self.assertEquals([fcurve5, fcurve3], group1.channels[:])
        self.assertEquals([fcurve5, fcurve3, fcurve2, fcurve4, fcurve0, fcurve1], channelbag.fcurves[:])


class DataPathTest(unittest.TestCase):
    def setUp(self):
        anims = bpy.data.actions
        while anims:
            anims.remove(anims[0])

    def test_repr(self):
        action = bpy.data.actions.new('TestAction')

        slot = action.slots.new()
        slot.identifier = 'OBTest'
        self.assertEqual("bpy.data.actions['TestAction'].slots[\"OBTest\"]", repr(slot))

        layer = action.layers.new(name="Layer")
        self.assertEqual("bpy.data.actions['TestAction'].layers[\"Layer\"]", repr(layer))

        strip = layer.strips.new(type='KEYFRAME')
        self.assertEqual("bpy.data.actions['TestAction'].layers[\"Layer\"].strips[0]", repr(strip))

        channelbag = strip.channelbags.new(slot)
        self.assertEqual("bpy.data.actions['TestAction'].layers[\"Layer\"].strips[0].channelbags[0]", repr(channelbag))


class VersioningTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "layered_action_versioning_42.blend"), load_ui=False)

    def test_nla_conversion(self):
        nla_object = bpy.data.objects["nla_object"]
        nla_anim_data = nla_object.animation_data
        self.assertTrue(nla_anim_data.action.is_action_layered)
        self.assertNotEqual(nla_anim_data.action_slot_handle, 0)

        # The action that is not pushed into an NLA strip.
        active_action = nla_anim_data.action
        strip = active_action.layers[0].strips[0]
        for fcurve_index, fcurve in enumerate(strip.channelbags[0].fcurves):
            self.assertEqual(fcurve.data_path, "rotation_euler")
            self.assertEqual(fcurve.group.name, "Object Transforms")
            self.assertEqual(fcurve.array_index, fcurve_index)

        self.assertEqual(len(nla_anim_data.nla_tracks), 2)
        self.assertTrue(nla_anim_data.nla_tracks[0].strips[0].action.is_action_layered)
        self.assertNotEqual(nla_anim_data.nla_tracks[0].strips[0].action_slot_handle, 0)

        self.assertTrue(nla_anim_data.nla_tracks[1].strips[0].action.is_action_layered)
        self.assertNotEqual(nla_anim_data.nla_tracks[1].strips[0].action_slot_handle, 0)

    def test_multi_use_action(self):
        object_a = bpy.data.objects["multi_user_object_a"]
        object_b = bpy.data.objects["multi_user_object_b"]
        self.assertTrue(object_a.animation_data.action.is_action_layered)
        self.assertNotEqual(object_a.animation_data.action_slot_handle, 0)

        self.assertTrue(object_b.animation_data.action.is_action_layered)
        self.assertNotEqual(object_b.animation_data.action_slot_handle, 0)

        self.assertEqual(object_a.animation_data.action, object_b.animation_data.action)
        self.assertEqual(object_a.animation_data.action_slot_handle, object_b.animation_data.action_slot_handle)

        action = object_a.animation_data.action
        strip = action.layers[0].strips[0]
        self.assertEqual(len(strip.channelbags[0].fcurves), 9)
        self.assertEqual(len(strip.channelbags[0].groups), 1)
        self.assertEqual(len(strip.channelbags[0].groups[0].channels), 9)

        # Multi user slots do not get named after their users.
        self.assertEqual(action.slots[0].identifier, "OBSlot")

    def test_action_constraint(self):
        constrained_object = bpy.data.objects["action_constraint_constrained"]
        action_constraint = constrained_object.constraints[0]
        self.assertTrue(action_constraint.action.is_action_layered)
        self.assertNotEqual(action_constraint.action_slot_handle, 0)

        action_owner_object = bpy.data.objects["action_constraint_action_owner"]
        action = action_owner_object.animation_data.action
        self.assertTrue(action.is_action_layered)
        self.assertEqual(action, action_constraint.action)
        self.assertEqual(action_owner_object.animation_data.action_slot_handle, action_constraint.action_slot_handle)
        strip = action.layers[0].strips[0]
        self.assertEqual(len(strip.channelbags[0].fcurves), 1)
        fcurve = strip.channelbags[0].fcurves[0]
        self.assertEqual(fcurve.data_path, "location")
        self.assertEqual(fcurve.array_index, 2)
        self.assertEqual(fcurve.group.name, "Object Transforms")

    def test_armature_action_conversion(self):
        armature_object = bpy.data.objects["armature_object"]
        action = armature_object.animation_data.action
        self.assertTrue(action.is_action_layered)
        strip = action.layers[0].strips[0]
        self.assertEqual(len(strip.channelbags[0].groups), 2)
        self.assertEqual(strip.channelbags[0].groups[0].name, "Bone")
        self.assertEqual(strip.channelbags[0].groups[1].name, "Bone.001")
        self.assertEqual(len(strip.channelbags[0].fcurves), 20)
        self.assertEqual(len(strip.channelbags[0].groups[0].channels), 10)
        self.assertEqual(len(strip.channelbags[0].groups[1].channels), 10)

        # Slots with a single user are named after their user.
        self.assertEqual(action.slots[0].identifier, "OBarmature_object")

        for fcurve in strip.channelbags[0].groups[0].channels:
            self.assertEqual(fcurve.group.name, "Bone")

        for fcurve in strip.channelbags[0].groups[1].channels:
            self.assertEqual(fcurve.group.name, "Bone.001")


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
