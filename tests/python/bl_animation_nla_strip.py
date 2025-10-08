# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Tests the evaluation of NLA strips based on their properties and placement on NLA tracks.

blender -b --factory-startup --python tests/python/bl_animation_nla_strip.py
"""

import bpy
import sys
import unittest


class AbstractNlaStripTest(unittest.TestCase):
    """ Sets up a series of strips in one NLA track. """

    test_object: bpy.types.Object = None
    """ Object whose X Location is animated to check strip evaluation. """

    nla_tracks: bpy.types.NlaTracks = None
    """ NLA tracks of the test object, which are cleared after each test case. """

    action: bpy.types.Action = None
    """ Action with X Location keyed on frames 1 to 4 with the same value as the frame, with constant interpolation. """

    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)

        self.test_object = bpy.data.objects.new(name="Object", object_data=bpy.data.meshes.new("Mesh"))
        bpy.context.collection.objects.link(self.test_object)
        self.test_object.animation_data_create()

        self.nla_tracks = self.test_object.animation_data.nla_tracks

        self.action = bpy.data.actions.new(name="ObjectAction")
        slot = self.action.slots.new(self.test_object.id_type, self.test_object.name)
        layer = self.action.layers.new("Layer")
        strip = layer.strips.new(type="KEYFRAME")
        channelbag = strip.channelbags.new(slot)

        x_location_fcurve = channelbag.fcurves.new(data_path="location", index=0, group_name="Object Transforms")
        for frame in range(1, 5):
            x_location_fcurve.keyframe_points.insert(frame, value=frame).interpolation = "CONSTANT"

    def add_strip_no_extrapolation(self, nla_track: bpy.types.NlaTrack, start: int) -> bpy.types.NlaStrip:
        """ Places a new strip with the test action on the given track, setting extrapolation to nothing. """
        strip = nla_track.strips.new("ObjectAction", start, self.action)
        strip.extrapolation = "NOTHING"
        return strip

    def assertFrameValue(self, frame: float, expected_value: float):
        """ Checks the evaluated X Location at the given frame. """
        int_frame, subframe = divmod(frame, 1)
        bpy.context.scene.frame_set(frame=int(int_frame), subframe=subframe)
        self.assertEqual(expected_value, self.test_object.evaluated_get(
            bpy.context.evaluated_depsgraph_get()
        ).matrix_world.translation[0])


class NlaStripSingleTest(AbstractNlaStripTest):
    """ Tests the inner values as well as the boundaries of one strip on one track. """

    def test_extrapolation_nothing(self):
        """ Tests one strip with no extrapolation. """
        self.add_strip_no_extrapolation(self.nla_tracks.new(), 1)

        self.assertFrameValue(0.9, 0.0)
        self.assertFrameValue(1.0, 1.0)
        self.assertFrameValue(1.1, 1.0)
        self.assertFrameValue(3.9, 3.0)
        self.assertFrameValue(4.0, 4.0)
        self.assertFrameValue(4.1, 0.0)


class NlaStripBoundaryTest(AbstractNlaStripTest):
    """ Tests two strips, the second one starting when the first one ends. """

    # Incorrectly, the first strip is currently evaluated at the boundary between two adjacent strips (see #113487).
    @unittest.expectedFailure
    def test_adjacent(self):
        """ The second strip should be evaluated at the boundary between two adjacent strips. """
        nla_track = self.nla_tracks.new()
        self.add_strip_no_extrapolation(nla_track, 1)
        self.add_strip_no_extrapolation(nla_track, 4)

        self.assertFrameValue(3.9, 3.0)
        self.assertFrameValue(4.0, 1.0)
        self.assertFrameValue(4.1, 1.0)

    def test_adjacent_muted(self):
        """ The first strip should be evaluated at the boundary if it is adjacent to a muted strip. """
        nla_track = self.nla_tracks.new()
        self.add_strip_no_extrapolation(nla_track, 1)
        self.add_strip_no_extrapolation(nla_track, 4).mute = True

        self.assertFrameValue(3.9, 3.0)
        self.assertFrameValue(4.0, 4.0)
        self.assertFrameValue(4.1, 0.0)

    def test_first_above_second(self):
        """ The first strip should be evaluated at the boundary, when followed by another strip on a track below. """
        self.add_strip_no_extrapolation(self.nla_tracks.new(), 4)
        self.add_strip_no_extrapolation(self.nla_tracks.new(), 1)

        self.assertFrameValue(3.9, 3.0)
        self.assertFrameValue(4.0, 4.0)
        self.assertFrameValue(4.1, 1.0)

    def test_second_above_first(self):
        """ The second strip should be evaluated at the boundary, when preceded by another strip on a track below. """
        self.add_strip_no_extrapolation(self.nla_tracks.new(), 1)
        self.add_strip_no_extrapolation(self.nla_tracks.new(), 4)

        self.assertFrameValue(3.9, 3.0)
        self.assertFrameValue(4.0, 1.0)
        self.assertFrameValue(4.1, 1.0)


class NLAStripActionSlotSelectionTest(AbstractNlaStripTest):
    def test_two_strips_for_same_action(self):
        action = bpy.data.actions.new("StripAction")
        action.slots.new('OBJECT', "Slot")
        self.assertTrue(action.is_action_layered)
        self.assertEqual(1, len(action.slots))

        track = self.nla_tracks.new()

        strip1 = track.strips.new("name", 1, action)
        self.assertEqual(action.slots[0], strip1.action_slot)
        self.assertEqual('OBJECT', action.slots[0].target_id_type, "Slot should have been rooted to object")

        strip2 = track.strips.new("name", 10, action)
        self.assertEqual(action.slots[0], strip2.action_slot)

    def test_switch_action_via_assignment(self):
        action1 = bpy.data.actions.new("StripAction 1")
        action1.slots.new('OBJECT', "Slot")
        self.assertTrue(action1.is_action_layered)
        self.assertEqual(1, len(action1.slots))

        action2 = bpy.data.actions.new("StripAction 2")
        action2.slots.new('OBJECT', "Slot")
        self.assertTrue(action2.is_action_layered)
        self.assertEqual(1, len(action2.slots))

        track = self.nla_tracks.new()

        strip = track.strips.new("name", 1, action1)
        self.assertEqual(action1.slots[0], strip.action_slot)
        self.assertEqual('OBJECT', action1.slots[0].target_id_type,
                         "Slot of Action 1 should have been rooted to object")

        strip.action = action2
        self.assertEqual(action2.slots[0], strip.action_slot)
        self.assertEqual('OBJECT', action2.slots[0].target_id_type,
                         "Slot of Action 2 should have been rooted to object")


if __name__ == "__main__":
    # Drop all arguments before "--", or everything if the delimiter is absent. Keep the executable path.
    unittest.main(argv=sys.argv[:1] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []))
