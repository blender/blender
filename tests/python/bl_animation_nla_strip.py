# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Tests the evaluation of NLA strips based on their properties and placement on NLA tracks.
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

    @classmethod
    def setUpClass(cls):
        bpy.ops.wm.read_factory_settings(use_empty=True)

        cls.test_object = bpy.data.objects.new(name="Object", object_data=bpy.data.meshes.new("Mesh"))
        bpy.context.collection.objects.link(cls.test_object)
        cls.test_object.animation_data_create()

        cls.nla_tracks = cls.test_object.animation_data.nla_tracks

        cls.action = bpy.data.actions.new(name="ObjectAction")
        x_location_fcurve = cls.action.fcurves.new(data_path="location", index=0, action_group="Object Transforms")
        for frame in range(1, 5):
            x_location_fcurve.keyframe_points.insert(frame, value=frame).interpolation = "CONSTANT"

    def tearDown(self):
        while len(self.nla_tracks):
            self.nla_tracks.remove(self.nla_tracks[0])

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


if __name__ == "__main__":
    # Drop all arguments before "--", or everything if the delimiter is absent. Keep the executable path.
    unittest.main(argv=sys.argv[:1] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []))
