# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import sys
import pathlib

import bpy

"""
blender -b --factory-startup --python tests/python/bl_animation_action.py -- --testdir tests/files/animation/
"""


class ActionSlotCreationTest(unittest.TestCase):
    """Test creating action slots & their resulting identifiers and id roots."""

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)

        self.action = bpy.data.actions.new('Action')

    def test_same_name_different_type(self):
        slot1 = self.action.slots.new('OBJECT', "Bob")
        slot2 = self.action.slots.new('CAMERA', "Bob")
        slot3 = self.action.slots.new('LIGHT', "Bob")

        self.assertEqual("OBBob", slot1.identifier)
        self.assertEqual('OBJECT', slot1.target_id_type)

        self.assertEqual("CABob", slot2.identifier)
        self.assertEqual('CAMERA', slot2.target_id_type)

        self.assertEqual("LABob", slot3.identifier)
        self.assertEqual('LIGHT', slot3.target_id_type)

    def test_same_name_same_type(self):
        slot1 = self.action.slots.new('OBJECT', "Bob")
        slot2 = self.action.slots.new('OBJECT', "Bob")
        slot3 = self.action.slots.new('OBJECT', "Bob")

        self.assertEqual("OBBob", slot1.identifier)
        self.assertEqual('OBJECT', slot1.target_id_type)

        self.assertEqual("OBBob.001", slot2.identifier)
        self.assertEqual('OBJECT', slot2.target_id_type)

        self.assertEqual("OBBob.002", slot3.identifier)
        self.assertEqual('OBJECT', slot3.target_id_type)

    def test_invalid_arguments(self):
        with self.assertRaises(TypeError):
            # ID type parameter is required.
            self.action.slots.new('Hello')

        with self.assertRaises(TypeError):
            # Name parameter is required.
            self.action.slots.new('OBJECT')

        with self.assertRaises(RuntimeError):
            # Name parameter must not be empty.
            self.action.slots.new('OBJECT', "")

        with self.assertRaises(TypeError):
            # Creating slots with unspecified ID type is
            # not supported in the Python API.
            self.action.slots.new('UNSPECIFIED', "Bob")

    def test_long_identifier(self):
        # Test a 257-character identifier, using a 255-character name. This is the
        # maximum length allowed (the DNA field is MAX_ID_NAME=258 long, which
        # includes the trailing zero byte).
        long_but_ok_name = (
            "This name is so long! It might look long, but it is just right! "
            "This name is so long! It might look long, but it is just right! "
            "This name is so long! It might look long, but it is just right! "
            "This name is so long! It might look long, but it is just right!"
        )
        assert (len(long_but_ok_name) == 255)
        slot_ok = self.action.slots.new('OBJECT', long_but_ok_name)
        self.assertEqual(long_but_ok_name, slot_ok.name_display, "this name should fit")
        self.assertEqual('OB' + long_but_ok_name, slot_ok.identifier, "this identifier should fit")

        # Test one character more.
        too_long_name = (
            "This name is so long! It might look long, and that it is indeed."
            "This name is so long! It might look long, and that it is indeed."
            "This name is so long! It might look long, and that it is indeed."
            "This name is so long! It might look long, and that it is indeed."
        )
        assert (len(too_long_name) == 256)
        too_long_name_truncated = too_long_name[:255]
        slot_long = self.action.slots.new('OBJECT', too_long_name)
        self.assertEqual(too_long_name_truncated, slot_long.name_display, "this name should be truncated")
        self.assertEqual('OB' + too_long_name_truncated, slot_long.identifier, "this identifier should be truncated")

        # Test with different trailing character.
        other_long_name = too_long_name[:-1] + "!"
        truncated_and_unique = other_long_name[:251] + ".001"
        slot_long2 = self.action.slots.new('OBJECT', too_long_name)
        self.assertEqual(truncated_and_unique, slot_long2.name_display,
                         "this name should be truncated and made unique")
        self.assertEqual('OB' + truncated_and_unique, slot_long2.identifier,
                         "this identifier should be truncated and made unique")


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
        slot_cube = action.slots.new(cube.id_type, cube.name)
        cube_adt.action_slot_handle = slot_cube.handle
        self.assertEqual(cube_adt.action_slot_handle, slot_cube.handle)

        # Assign the Action to the camera as well.
        camera = bpy.data.objects['Camera']
        slot_camera = action.slots.new(camera.id_type, camera.name)
        camera_adt = camera.animation_data_create()
        camera_adt.action = action
        self.assertEqual(camera_adt.action_slot_handle, slot_camera.handle)

        # Unassigning should keep the slot identifier.
        cube_adt.action = None
        self.assertEqual(cube_adt.last_slot_identifier, slot_cube.identifier)

        # It should not be possible to set the slot handle while the Action is unassigned.
        slot_extra = action.slots.new('OBJECT', "Slot")
        cube_adt.action_slot_handle = slot_extra.handle
        self.assertNotEqual(cube_adt.action_slot_handle, slot_extra.handle)

        # Slots from another Action should be gracefully rejected.
        other_action = bpy.data.actions.new("That Other Action")
        slot = other_action.slots.new('OBJECT', "Slot")
        cube_adt.action = action
        cube_adt.action_slot = slot_cube
        with self.assertRaises(RuntimeError):
            cube_adt.action_slot = slot
        self.assertEqual(cube_adt.action_slot, slot_cube, "The slot should not have changed")

    def test_slot_users(self):
        action = bpy.data.actions.new('TestAction')
        self.assertEqual(0, action.users)

        # Assign the Action to Cube.
        cube = bpy.data.objects['Cube']
        cube_adt = cube.animation_data_create()
        cube_adt.action = action
        slot_cube = action.slots.new(cube.id_type, cube.name)
        cube_adt.action_slot = slot_cube

        self.assertEqual([cube], slot_cube.users())

        # Assign the same slot to the Camera object as well.
        camera = bpy.data.objects['Camera']
        camera_adt = camera.animation_data_create()
        camera_adt.action = action
        camera_adt.action_slot = slot_cube

        # Sort by name, as the order doesn't matter and is an implementation detail.
        self.assertEqual([camera, cube], sorted(slot_cube.users(), key=lambda id: id.name))

    def test_untyped_slot_assignment_local(self):
        """Test untyped slot assignment, with a local Action."""

        action = self._load_legacy_action(link=False)

        # Assign the Action to a Mesh data-block. This should set the ID type of the Slot to 'MESH'.
        mesh = bpy.data.meshes['Cube']
        mesh.animation_data_create().action = action

        slot = action.slots[0]
        self.assertEqual('MESH', slot.target_id_type, "After assignment, the ID type should be specified.")
        self.assertEqual("MELegacy Slot", slot.identifier)

    def test_untyped_slot_assignment_linked(self):
        """Test untyped slot assignment, with a linked Action."""

        action = self._load_legacy_action(link=True)

        # Assign the Action to a Mesh data-block. This should set the ID type of the Slot to 'MESH'.
        mesh = bpy.data.meshes['Cube']
        mesh.animation_data_create().action = action

        slot = action.slots[0]
        self.assertEqual(
            'UNSPECIFIED',
            slot.target_id_type,
            "After assignment, the ID type should remain UNSPECIFIED when the Action is linked.")
        self.assertEqual("XXLegacy Slot", slot.identifier)

    def test_slot_identifier_writing(self):
        """Test writing to the identifier of a slot."""

        action = self._load_legacy_action(link=False)

        slot_1 = action.slots[0]
        slot_2 = action.slots.new('OBJECT', "Slot")

        self.assertEqual("XXLegacy Slot", slot_1.identifier)
        self.assertEqual('UNSPECIFIED', slot_1.target_id_type)
        self.assertEqual("OBSlot", slot_2.identifier)
        self.assertEqual('OBJECT', slot_2.target_id_type)

        # Assigning identifier with same type prefix should work.
        slot_1.identifier = "XXCoolerSlot"
        slot_2.identifier = "OBCoolerSlot"
        self.assertEqual("XXCoolerSlot", slot_1.identifier)
        self.assertEqual("OBCoolerSlot", slot_2.identifier)

        # Assigning identifier with different type prefix should still set the
        # name part, but leave the type prefix untouched so that it stays
        # consistent with the actual target ID type of the slot.
        slot_1.identifier = "MAEvenCoolerSlot"
        slot_2.identifier = "MAEvenCoolerSlot"
        self.assertEqual("XXEvenCoolerSlot", slot_1.identifier)
        self.assertEqual("OBEvenCoolerSlot", slot_2.identifier)

    def test_untyped_slot_target_id_writing(self):
        """Test writing to the target id type of an untyped slot."""

        action = self._load_legacy_action(link=False)

        slot = action.slots[0]
        self.assertEqual('UNSPECIFIED', slot.target_id_type)
        self.assertEqual("XXLegacy Slot", slot.identifier)

        slot.target_id_type = 'OBJECT'

        self.assertEqual(
            'OBJECT',
            slot.target_id_type,
            "Should be able to write to target_id_type of a slot when not yet specified.")
        self.assertEqual("OBLegacy Slot", slot.identifier)

        slot.target_id_type = 'MATERIAL'

        self.assertEqual(
            'OBJECT',
            slot.target_id_type,
            "Should NOT be able to write to target_id_type of a slot when already specified.")
        self.assertEqual("OBLegacy Slot", slot.identifier)

    def test_untyped_slot_target_id_writing_with_duplicate_identifier(self):
        """Test that writing to the target id type a slot appropriately renames
        it when that would otherwise cause its identifier to collide with an
        already existing slot."""

        action = self._load_legacy_action(link=False)

        slot = action.slots[0]

        # Create soon-to-collide slot.
        other_slot = action.slots.new('OBJECT', "Legacy Slot")

        # Ensure the setup is correct.
        self.assertEqual('UNSPECIFIED', slot.target_id_type)
        self.assertEqual("XXLegacy Slot", slot.identifier)
        self.assertEqual('OBJECT', other_slot.target_id_type)
        self.assertEqual("OBLegacy Slot", other_slot.identifier)

        # Assign the colliding target id type.
        slot.target_id_type = 'OBJECT'

        self.assertEqual('OBJECT', slot.target_id_type)
        self.assertEqual(
            "OBLegacy Slot.001",
            slot.identifier,
            "Should get renamed to not conflict with existing slots.")
        self.assertEqual('OBJECT', other_slot.target_id_type)
        self.assertEqual("OBLegacy Slot", other_slot.identifier)

    @staticmethod
    def _load_legacy_action(*, link: bool) -> bpy.types.Action:
        # At the moment of writing, the only way to create an untyped slot is to
        # load a legacy Action that has `id_root=0` and let the versioning code
        # create the untyped slot.
        blendpath = args.testdir / "legacy-action-without-idroot.blend"

        # Append or link the one Action from the legacy file.
        with bpy.data.libraries.load(str(blendpath), link=link) as (data_in, data_out):
            data_out.actions = data_in.actions

        # Using plain asserts here, because these are not part of the unit test.
        # They're here to test that the test code itself is doing the right thing.
        assert len(data_out.actions) == 1
        assert isinstance(data_out.actions[0], bpy.types.Action)

        # Check that the state of things is as expected.
        action = data_out.actions[0]
        slot = action.slots[0]
        assert slot.target_id_type == 'UNSPECIFIED'
        assert slot.identifier == "XXLegacy Slot"

        return action


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


class ChannelbagsTest(unittest.TestCase):
    def setUp(self):
        anims = bpy.data.actions
        while anims:
            anims.remove(anims[0])

        self.action = bpy.data.actions.new('TestAction')

        self.slot = self.action.slots.new('OBJECT', "Test")

        self.layer = self.action.layers.new(name="Layer")
        self.strip = self.layer.strips.new(type='KEYFRAME')

    def test_create_remove_channelbag(self):
        channelbag = self.strip.channelbags.new(self.slot)

        self.strip.key_insert(self.slot, "location", 1, 47.0, 327.0)
        self.assertEqual("location", channelbag.fcurves[0].data_path,
                         "Keys for the channelbag's slot should go into the channelbag")
        self.assertEqual(self.slot, channelbag.slot)

        self.strip.channelbags.remove(channelbag)
        self.assertEqual([], list(self.strip.channelbags))

    def test_ensure_channelbag(self):
        channelbag = self.strip.channelbag(self.slot, ensure=False)
        self.assertIsNone(channelbag)
        self.assertEqual([], list(self.strip.channelbags))

        channelbag = self.strip.channelbag(self.slot, ensure=True)
        self.assertEqual([channelbag], list(self.strip.channelbags))
        self.assertEqual(self.slot, channelbag.slot)

    def test_ensure_fcurve(self):
        channelbag = self.strip.channelbag(self.slot, ensure=True)
        self.assertEqual([], channelbag.fcurves[:])

        fcurve_1 = channelbag.fcurves.ensure("location", index=2, group_name="Name")
        self.assertEqual("location", fcurve_1.data_path)
        self.assertEqual(2, fcurve_1.array_index)
        self.assertEqual("Name", fcurve_1.group.name)
        self.assertIn("Name", channelbag.groups)
        self.assertEqual([fcurve_1], channelbag.fcurves[:])

        fcurve_2 = channelbag.fcurves.ensure("location", index=2, group_name="Name")
        self.assertEqual(fcurve_1, fcurve_2)
        self.assertEqual([fcurve_1], channelbag.fcurves[:])

    def test_create_remove_fcurves(self):
        channelbag = self.strip.channelbags.new(self.slot)

        # Creating an F-Curve should work.
        fcurve = channelbag.fcurves.new('location', index=1)
        self.assertIsNotNone(fcurve)
        self.assertEqual(fcurve.data_path, 'location')
        self.assertEqual(fcurve.array_index, 1)
        self.assertEqual([fcurve], channelbag.fcurves[:])

        # Empty data paths should not be accepted.
        with self.assertRaises(RuntimeError):
            channelbag.fcurves.new('', index=1)
        self.assertEqual([fcurve], channelbag.fcurves[:])

        # Creating an F-Curve twice should fail:
        with self.assertRaises(RuntimeError):
            channelbag.fcurves.new('location', index=1)
        self.assertEqual([fcurve], channelbag.fcurves[:])

        # Removing an unrelated F-Curve should fail, even when an F-Curve with
        # the same RNA path and array index exists.
        other_slot = self.action.slots.new('OBJECT', "Slot")
        other_cbag = self.strip.channelbags.new(other_slot)
        other_fcurve = other_cbag.fcurves.new('location', index=1)
        with self.assertRaises(RuntimeError):
            channelbag.fcurves.remove(other_fcurve)
        self.assertEqual([fcurve], channelbag.fcurves[:])

        # Removing an existing F-Curve should work:
        channelbag.fcurves.remove(fcurve)
        self.assertEqual([], channelbag.fcurves[:])

    def test_fcurves_clear(self):
        channelbag = self.strip.channelbags.new(self.slot)

        for index in range(4):
            channelbag.fcurves.new('rotation_quaternion', index=index)

        self.assertEqual(4, len(channelbag.fcurves))
        channelbag.fcurves.clear()
        self.assertEqual([], channelbag.fcurves[:])

    def test_channel_groups(self):
        channelbag = self.strip.channelbags.new(self.slot)

        # Create some fcurves to play with.
        fcurve0 = channelbag.fcurves.new('location', index=0)
        fcurve1 = channelbag.fcurves.new('location', index=1)
        fcurve2 = channelbag.fcurves.new('location', index=2)
        fcurve3 = channelbag.fcurves.new('scale', index=0)
        fcurve4 = channelbag.fcurves.new('scale', index=1)
        fcurve5 = channelbag.fcurves.new('scale', index=2)

        self.assertEqual([], channelbag.groups[:])

        # Create some channel groups.
        group0 = channelbag.groups.new('group0')
        group1 = channelbag.groups.new('group1')
        self.assertEqual([group0, group1], channelbag.groups[:])
        self.assertEqual([], group0.channels[:])
        self.assertEqual([], group1.channels[:])

        # Assign some fcurves to the channel groups. Intentionally not in order
        # so we can test that the fcurves get moved around properly.
        fcurve5.group = group1
        fcurve3.group = group1
        fcurve2.group = group0
        fcurve4.group = group0
        self.assertEqual([fcurve2, fcurve4], group0.channels[:])
        self.assertEqual([fcurve5, fcurve3], group1.channels[:])
        self.assertEqual([fcurve2, fcurve4, fcurve5, fcurve3, fcurve0, fcurve1], channelbag.fcurves[:])

        # Weird case to be consistent with the legacy API: assigning None to an
        # fcurve's group does *not* unassign it from its group. This is stupid,
        # and we should change it at some point.  But it's how the legacy API
        # already works (presumably an oversight), so sticking to that for now.
        fcurve3.group = None
        self.assertEqual(group1, fcurve3.group)
        self.assertEqual([fcurve2, fcurve4], group0.channels[:])
        self.assertEqual([fcurve5, fcurve3], group1.channels[:])
        self.assertEqual([fcurve2, fcurve4, fcurve5, fcurve3, fcurve0, fcurve1], channelbag.fcurves[:])

        # Removing a group.
        channelbag.groups.remove(group0)
        self.assertEqual([group1], channelbag.groups[:])
        self.assertEqual([fcurve5, fcurve3], group1.channels[:])
        self.assertEqual([fcurve5, fcurve3, fcurve2, fcurve4, fcurve0, fcurve1], channelbag.fcurves[:])

        # Attempting to remove a channel group that belongs to a different
        # channel bag should fail.
        other_slot = self.action.slots.new('OBJECT', "Slot")
        other_cbag = self.strip.channelbags.new(other_slot)
        other_group = other_cbag.groups.new('group1')
        with self.assertRaises(RuntimeError):
            channelbag.groups.remove(other_group)

        # Another weird case that we reproduce from the legacy API: attempting
        # to assign a group to an fcurve that doesn't belong to the same channel
        # bag should silently fail (just does a printf to stdout).
        fcurve0.group = other_group
        self.assertEqual([group1], channelbag.groups[:])
        self.assertEqual([fcurve5, fcurve3], group1.channels[:])
        self.assertEqual([fcurve5, fcurve3, fcurve2, fcurve4, fcurve0, fcurve1], channelbag.fcurves[:])

    def test_channelbag_slot_properties(self):
        slot_1 = self.slot
        slot_2 = self.action.slots.new('MATERIAL', "Test2")
        slot_3 = self.action.slots.new('CAMERA', "Test3")

        channelbag_1 = self.strip.channelbags.new(slot_1)
        channelbag_2 = self.strip.channelbags.new(slot_2)
        channelbag_3 = self.strip.channelbags.new(slot_3)

        self.assertEqual(slot_1.handle, channelbag_1.slot_handle)
        self.assertEqual(slot_1, channelbag_1.slot)

        self.assertEqual(slot_2.handle, channelbag_2.slot_handle)
        self.assertEqual(slot_2, channelbag_2.slot)

        self.assertEqual(slot_3.handle, channelbag_3.slot_handle)
        self.assertEqual(slot_3, channelbag_3.slot)


class DataPathTest(unittest.TestCase):
    def setUp(self):
        anims = bpy.data.actions
        while anims:
            anims.remove(anims[0])

    def test_repr(self):
        action = bpy.data.actions.new('TestAction')

        slot = action.slots.new('OBJECT', "Test")
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

        # Slots created from legacy Actions are always called "Legacy SLot".
        self.assertEqual(action.slots[0].identifier, "OBLegacy Slot")

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

        # Slots on converted Actions are always called "Legacy Slot"
        self.assertEqual(action.slots[0].identifier, "OBLegacy Slot")

        for fcurve in strip.channelbags[0].groups[0].channels:
            self.assertEqual(fcurve.group.name, "Bone")

        for fcurve in strip.channelbags[0].groups[1].channels:
            self.assertEqual(fcurve.group.name, "Bone.001")


class SlotHandleLibraryOverridesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        args.output_dir.mkdir(parents=True, exist_ok=True)

        cls.libfile = args.testdir.resolve() / "liboverride-action-slot-libfile.blend"
        cls.workfile = args.output_dir.resolve() / "liboverride-action-slot.blend"

    @classmethod
    def tearDownClass(cls):
        cls.workfile.unlink(missing_ok=True)

    def test_liboverride_slot_handle(self):
        # Whenever a liboverride changes the assigned Action, there should be a
        # liboverride on the slot handle as well. Even when the assigned slot in
        # the original data numerically has the same handle as the overridden
        # slot.

        self._create_test_file()
        self._load_test_file()
        self._check_assumptions()
        self._perform_test()

    def _create_test_file(self):
        """Create the test file.

        This has to happen every time the test runs, because it's about the
        creation of library override operations. Creating the file once, storing
        it with the rest of the test files, and opening it here to test it, will
        just repeat the test on a once-written-correctly file, and not test the
        currently-running Blender.
        """

        bpy.ops.wm.read_homefile(use_factory_startup=True, use_empty=True)

        # Link Suzanne into the file and then into the scene.
        with bpy.data.libraries.load(str(self.libfile), link=True, relative=False) as (data_from, data_to):
            data_to.objects = ['Library Suzanne']
        orig_lib_suzanne = data_to.objects[0]
        bpy.context.scene.collection.objects.link(orig_lib_suzanne)

        # Create a library override on Suzanne.
        with bpy.context.temp_override(active_object=orig_lib_suzanne):
            bpy.ops.object.make_override_library()

        # Create a local Action to assign.
        local_action = bpy.data.actions.new("Local Action")
        local_slot = local_action.slots.new('OBJECT', "Local Slot")
        layer = local_action.layers.new("Layer")
        strip = layer.strips.new(type='KEYFRAME')
        cbag = strip.channelbags.new(local_slot)
        fcurve = cbag.fcurves.new('location', index=2)
        fcurve.keyframe_points.insert(1, -5)
        fcurve.keyframe_points.insert(20, 5)

        # Grab the overridden Suzanne, and assign the local Action + a slot from that Action.
        override_suzanne = bpy.data.objects['Library Suzanne', None]
        override_suzanne.animation_data.action = local_action
        override_suzanne.animation_data.action_slot = local_slot

        # Save the file to disk.
        bpy.ops.wm.save_as_mainfile(filepath=str(self.workfile), check_existing=False)

    def _load_test_file(self):
        bpy.ops.wm.read_homefile(use_factory_startup=True)  # Just to be sure.
        bpy.ops.wm.open_mainfile(filepath=str(self.workfile), load_ui=False)

    def _check_assumptions(self):
        """Check that the test data is indeed as expected."""

        # The library Action and the local Action should have the same handle on
        # the first slot. If the slot handles are different, Blender's default
        # library override diffing code would create an override operation, and
        # this test will produce a false positive.
        self.assertEqual(
            bpy.data.actions['Library Action'].slots[0].handle,
            bpy.data.actions['Local Action'].slots[0].handle,
        )

        # The library & local Action slots should have different identifiers.
        # Otherwise the slot assignment will be correct regardless of library
        # overrides, and this test will produce a false positive.
        self.assertNotEqual(
            bpy.data.actions['Library Action'].slots[0].identifier,
            bpy.data.actions['Local Action'].slots[0].identifier,
        )

        # Check the Action assignments before we trust a check for the action slot.
        libpath = bpy.data.libraries['liboverride-action-slot-libfile.blend'].filepath
        orig_lib_suzanne = bpy.data.objects['Library Suzanne', libpath]
        override_suzanne = bpy.data.objects['Library Suzanne', None]

        self.assertEqual(bpy.data.actions['Library Action'], orig_lib_suzanne.animation_data.action)
        self.assertEqual(bpy.data.actions['Local Action'], override_suzanne.animation_data.action)

    def _perform_test(self):
        override_suzanne = bpy.data.objects['Library Suzanne', None]

        # === The actual test ===
        self.assertEqual(bpy.data.actions['Local Action'].slots[0], override_suzanne.animation_data.action_slot)

        # Set Suzanne's Z position to something large, and go the first frame to
        # let the animation system evaluation overwrite it.
        bpy.context.scene.frame_set(1)
        self.assertLess(override_suzanne.location.z,
                        -1, "Suzanne should be significantly below Z=0 when animated by the library Action")


class ConvenienceFunctionsTest(unittest.TestCase):

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)

        self.action = bpy.data.actions.new('Action')

    def test_fcurve_ensure_for_datablock(self) -> None:
        # The function should be None-safe.
        with self.assertRaises(TypeError):
            self.action.fcurve_ensure_for_datablock(None, "location")
        self.assertEqual(0, len(self.action.layers))
        self.assertEqual(0, len(self.action.slots))

        # The function should not work unless the Action is assigned to its target.
        ob_cube = bpy.data.objects["Cube"]
        with self.assertRaises(RuntimeError):
            self.action.fcurve_ensure_for_datablock(ob_cube, "location")
        self.assertEqual(0, len(self.action.layers))
        self.assertEqual(0, len(self.action.slots))

        # The function should not work on empty data paths.
        adt = ob_cube.animation_data_create()
        adt.action = self.action
        with self.assertRaises(RuntimeError):
            self.action.fcurve_ensure_for_datablock(ob_cube, "")
        self.assertEqual(0, len(self.action.layers))
        self.assertEqual(0, len(self.action.slots))

        # And finally the happy flow.
        fcurve = self.action.fcurve_ensure_for_datablock(ob_cube, "location", index=2)
        self.assertEqual(1, len(self.action.layers))
        self.assertEqual(1, len(self.action.layers[0].strips))
        self.assertEqual('KEYFRAME', self.action.layers[0].strips[0].type)
        self.assertEqual(1, len(self.action.slots))
        self.assertEqual("location", fcurve.data_path)
        self.assertEqual(2, fcurve.array_index)

        channelbag = self.action.layers[0].strips[0].channelbags[0]
        self.assertEqual(fcurve, channelbag.fcurves[0])

    def test_fcurve_ensure_for_datablock_group_name(self) -> None:
        # Assign the Action to the Cube.
        ob_cube = bpy.data.objects["Cube"]
        adt = ob_cube.animation_data_create()
        adt.action = self.action

        with self.assertRaises(IndexError):
            self.action.layers[0].strips[0].channelbags[0]

        fcurve = self.action.fcurve_ensure_for_datablock(ob_cube, "location", index=2, group_name="grúpa")

        channelbag = self.action.layers[0].strips[0].channelbags[0]
        self.assertEqual(fcurve, channelbag.fcurves[0])

        # Check that the group was also created correctly.
        self.assertIn("grúpa", channelbag.groups)
        self.assertIn(fcurve, channelbag.groups["grúpa"].channels[:])


def main():
    global args
    import argparse

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        type=pathlib.Path,
        default=pathlib.Path("."),
        help="Where to output temp saved blendfiles",
        required=False,
    )

    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
