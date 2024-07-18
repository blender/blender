# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import sys

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

        # Unassigning should keep the slot name.
        cube_adt.action = None
        self.assertEqual(cube_adt.action_slot_name, slot_cube.name)

        # It should not be possible to set the slot handle while the Action is unassigned.
        slot_extra = action.slots.new()
        cube_adt.action_slot_handle = slot_extra.handle
        self.assertNotEqual(cube_adt.action_slot_handle, slot_extra.handle)


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


class TestLegacyLayered(unittest.TestCase):
    """Test boundaries between legacy & layered Actions.

    Layered functionality should not be available on legacy actions, and vice versa.
    """

    def test_legacy_action(self) -> None:
        """Test layered operations on a legacy Action"""

        act = bpy.data.actions.new('LegacyAction')
        act.fcurves.new("location", index=0)  # Add an FCurve to make this a non-empty legacy Action.
        self.assertTrue(act.is_action_legacy)
        self.assertFalse(act.is_action_layered)
        self.assertFalse(act.is_empty)

        # Adding a layer should be prevented.
        with self.assertRaises(RuntimeError):
            act.layers.new("laagje")
        self.assertSequenceEqual([], act.layers)

        # Adding a slot should be prevented.
        with self.assertRaises(RuntimeError):
            act.slots.new()
        self.assertSequenceEqual([], act.slots)

    def test_layered_action(self) -> None:
        """Test legacy operations on a layered Action"""

        act = bpy.data.actions.new('LayeredAction')
        act.layers.new("laagje")  # Add a layer to make this a non-empty legacy Action.
        self.assertFalse(act.is_action_legacy)
        self.assertTrue(act.is_action_layered)
        self.assertFalse(act.is_empty)

        # Adding an FCurve should be prevented.
        with self.assertRaises(RuntimeError):
            act.fcurves.new("location", index=0)
        self.assertSequenceEqual([], act.fcurves)

        # Adding an ActionGroup should be prevented.
        with self.assertRaises(RuntimeError):
            act.groups.new("groepie")
        self.assertSequenceEqual([], act.groups)


class ChannelBagsTest(unittest.TestCase):
    def setUp(self):
        anims = bpy.data.actions
        while anims:
            anims.remove(anims[0])

        self.action = bpy.data.actions.new('TestAction')

        self.slot = self.action.slots.new()
        self.slot.name = 'OBTest'

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


class DataPathTest(unittest.TestCase):
    def setUp(self):
        anims = bpy.data.actions
        while anims:
            anims.remove(anims[0])

    def test_repr(self):
        action = bpy.data.actions.new('TestAction')

        slot = action.slots.new()
        slot.name = 'OBTest'
        self.assertEqual("bpy.data.actions['TestAction'].slots[\"OBTest\"]", repr(slot))

        layer = action.layers.new(name="Layer")
        self.assertEqual("bpy.data.actions['TestAction'].layers[\"Layer\"]", repr(layer))

        strip = layer.strips.new(type='KEYFRAME')
        self.assertEqual("bpy.data.actions['TestAction'].layers[\"Layer\"].strips[0]", repr(strip))

        channelbag = strip.channelbags.new(slot)
        self.assertEqual("bpy.data.actions['TestAction'].layers[\"Layer\"].strips[0].channelbags[0]", repr(channelbag))


def main():
    global args
    import argparse

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    parser = argparse.ArgumentParser()
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
