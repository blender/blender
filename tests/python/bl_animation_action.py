# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import sys

import bpy

"""
blender -b --factory-startup --python tests/python/bl_animation_action.py
"""


def enable_experimental_animation_baklava():
    bpy.context.preferences.view.show_developer_ui = True
    bpy.context.preferences.experimental.use_animation_baklava = True


def disable_experimental_animation_baklava():
    bpy.context.preferences.view.show_developer_ui = False
    bpy.context.preferences.experimental.use_animation_baklava = False


class ActionSlotAssignmentTest(unittest.TestCase):
    """Test assigning actions & check reference counts."""

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        enable_experimental_animation_baklava()

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
        enable_experimental_animation_baklava()

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


class TestLegacyLayered(unittest.TestCase):
    """Test boundaries between legacy & layered Actions.

    Layered functionality should not be available on legacy actions.
    """

    def test_legacy_action(self) -> None:
        """Test layered operations on a legacy Action"""

        # Disable Baklava's backward-compatibility with the legacy API to create an actual legacy Action.
        disable_experimental_animation_baklava()

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

        # Adding an ActionGroup should be prevented, at least until grouping is supported.
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
