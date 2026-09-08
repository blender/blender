# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import bpy
import sys

"""
blender --background --python tests/python/bl_animation_context_markers.py -- --verbose
"""


def _add_test_markers(collection: bpy.types.TimelineMarkers | bpy.types.ActionPoseMarkers):
    """Adds a set of test markers to the given collection."""

    marker = collection.new("Foo")
    marker.frame = 10
    marker.select = False

    marker = collection.new("Bar")
    marker.frame = 20
    marker.select = True

    marker = collection.new("Baz")
    marker.frame = 30
    marker.select = False


class ContextMarkersTest(unittest.TestCase):
    def setUp(self):
        super().setUp()
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        self.scene = bpy.data.scenes['Scene']
        self.cube = bpy.data.objects['Cube']

    def _test_context_no_markers(self):
        """Asserts that the current context has no markers in it."""
        markers = bpy.context.markers
        self.assertEqual(0, len(markers))

        selected_markers = bpy.context.selected_markers
        self.assertEqual(0, len(selected_markers))

    def _test_context_markers(self):
        """Asserts that the current context contains the markers created by `_add_test_markers`."""
        markers = bpy.context.markers
        self.assertEqual(3, len(markers))
        self.assertEqual("Foo", markers[0].name)
        self.assertEqual("Bar", markers[1].name)
        self.assertEqual("Baz", markers[2].name)
        self.assertEqual(10, markers[0].frame)
        self.assertEqual(20, markers[1].frame)
        self.assertEqual(30, markers[2].frame)
        self.assertEqual(False, markers[0].select)
        self.assertEqual(True, markers[1].select)
        self.assertEqual(False, markers[2].select)

        selected_markers = bpy.context.selected_markers
        self.assertEqual(1, len(selected_markers))
        self.assertEqual("Bar", selected_markers[0].name)
        self.assertEqual(20, selected_markers[0].frame)
        self.assertEqual(True, selected_markers[0].select)


class SceneMarkersTest(ContextMarkersTest):
    """Tests for markers in the current scene."""

    def test_scene_markers(self):
        self._test_context_no_markers()
        _add_test_markers(self.scene.timeline_markers)
        self._test_context_markers()


class PoseMarkersTest(ContextMarkersTest):
    """Helper for testing pose markers."""

    def _test_pose_markers(self, space_mode, animation_data_owner):
        action = bpy.data.actions.new("Action")
        _add_test_markers(action.pose_markers)

        # To access pose markers, we need to...
        #   1) be in context of a dopesheet editor,
        #   2) editor is in "Action" mode,
        #   3) have an active action, and
        #   4) editor has pose markers enabled.
        # Between each step, we will ensure that we don't have access to any markers yet, and no errors occur.
        window = bpy.context.window
        screen = window.screen
        area = screen.areas[0]
        area.type = 'DOPESHEET_EDITOR'

        region = next(r for r in area.regions if r.type == 'WINDOW')
        space = area.spaces.active

        with bpy.context.temp_override(window=window, screen=screen, area=area, region=region):
            self._test_context_no_markers()

        space.mode = space_mode

        with bpy.context.temp_override(window=window, screen=screen, area=area, region=region):
            self._test_context_no_markers()

        # This ensures we have an active action in the animcontext.
        animation_data_owner.animation_data_create()
        animation_data_owner.animation_data.action = action

        with bpy.context.temp_override(window=window, screen=screen, area=area, region=region):
            self._test_context_no_markers()

        space.show_pose_markers = True

        with bpy.context.temp_override(window=window, screen=screen, area=area, region=region):
            self._test_context_markers()


class ActionPoseMarkersTest(PoseMarkersTest):
    """Tests for pose markers on an object action."""

    def test_action_pose_markers(self):
        self._test_pose_markers('ACTION', self.cube)


class ShapeKeyPoseMarkersTest(PoseMarkersTest):
    """Tests for pose markers on a shape key action."""

    def test_shape_key_pose_markers(self):
        # Object needs at least one shapekey before it can be assigned a shapekey action.
        self.cube.shape_key_add()

        self._test_pose_markers('SHAPEKEY', self.cube.data.shape_keys)


class OverrideSceneMarkersTest(ContextMarkersTest):
    """Tests for markers in a context-overridden scene (used for fetching sequencer scene markers for example)."""

    def test_override_scene_markers(self):
        other_scene = bpy.data.scenes.new("Other")
        _add_test_markers(other_scene.timeline_markers)

        self._test_context_no_markers()
        with bpy.context.temp_override(scene=other_scene):
            self._test_context_markers()


if __name__ == "__main__":
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
