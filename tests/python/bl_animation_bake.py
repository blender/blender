# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import sys
import pathlib

import bpy
from bpy_extras import anim_utils


"""
blender -b --factory-startup --python tests/python/bl_animation_bake.py
"""

OBJECT_BAKE_OPTIONS = anim_utils.BakeOptions(
    only_selected=False,
    do_pose=False,
    do_object=True,
    do_visual_keying=False,
    do_constraint_clear=False,
    do_parents_clear=False,
    do_clean=False,
    do_location=True,
    do_rotation=True,
    do_scale=True,
    do_bbone=False,
    do_custom_props=False,
)


class ObjectBakeTest(unittest.TestCase):
    """This tests the animation baking to document the current behavior without any attempt of declaring that behavior correct or good."""
    obj: bpy.types.Object

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        self.obj = bpy.data.objects.new("test_object", None)
        bpy.context.scene.collection.objects.link(self.obj)
        self.obj.animation_data_create()

    def test_bake_object_without_animation(self):
        self.assertEqual(self.obj.animation_data.action, None)

        anim_utils.bake_action_objects([(self.obj, None)], frames=range(0, 10), bake_options=OBJECT_BAKE_OPTIONS)

        action = self.obj.animation_data.action
        self.assertIsNotNone(action, "Baking without an existing action should create an action")
        self.assertEqual(len(action.slots), 1, "Baking should have created a slot")
        self.assertEqual(action.slots[0], self.obj.animation_data.action_slot)
        channelbag = anim_utils.action_get_channelbag_for_slot(action, action.slots[0])

        self.assertIsNotNone(channelbag)
        self.assertEqual(len(channelbag.fcurves), 9, "If no animation is present, FCurves are created for all channels")

        for fcurve in channelbag.fcurves:
            self.assertEqual(len(fcurve.keyframe_points), 10, f"Unexpected key count on {fcurve.data_path}")
            self.assertAlmostEqual(fcurve.keyframe_points[0].co.x, 0, 6,
                                   f"Unexpected key y position on {fcurve.data_path}")
            self.assertAlmostEqual(fcurve.keyframe_points[-1].co.x, 9, 6, "Baking range is exclusive for the end")

    def test_bake_object_animation_to_new_action(self):
        action = bpy.data.actions.new("test_action")
        self.obj.animation_data.action = action

        bpy.context.scene.frame_set(0)
        self.obj.keyframe_insert("location")
        bpy.context.scene.frame_set(15)
        self.obj.location = (1, 1, 1)
        self.obj.keyframe_insert("location")

        # Passing None here will create a new action.
        anim_utils.bake_action_objects([(self.obj, None)], frames=range(0, 10), bake_options=OBJECT_BAKE_OPTIONS)

        self.assertNotEqual(action, self.obj.animation_data.action, "Expected baking to result in a new action")
        baked_action = self.obj.animation_data.action
        self.assertEqual(len(baked_action.slots), 1)
        self.assertEqual(baked_action.slots[0].name_display, action.slots[0].name_display)
        channelbag = anim_utils.action_get_channelbag_for_slot(baked_action, self.obj.animation_data.action_slot)

        self.assertIsNotNone(channelbag)
        self.assertEqual(len(channelbag.fcurves), 9)

        for fcurve in channelbag.fcurves:
            self.assertEqual(len(fcurve.keyframe_points), 10, f"Unexpected key count on {fcurve.data_path}")
            self.assertAlmostEqual(fcurve.keyframe_points[-1].co.x, 9,
                                   6, f"Baking to a new action should delete all keys outside the given range ({fcurve.data_path})")

    def test_bake_object_animation_to_existing_action(self):
        action = bpy.data.actions.new("test_action")
        self.obj.animation_data.action = action

        bpy.context.scene.frame_set(0)
        self.obj.keyframe_insert("location")
        bpy.context.scene.frame_set(15)
        self.obj.location = (1, 1, 1)
        self.obj.keyframe_insert("location")

        # Passing the action as the second element of the tuple means that it will be written into.
        anim_utils.bake_action_objects([(self.obj, action)], frames=range(0, 10), bake_options=OBJECT_BAKE_OPTIONS)

        self.assertEqual(self.obj.animation_data.action, action)
        self.assertEqual(len(action.slots), 1)
        channelbag = anim_utils.action_get_channelbag_for_slot(action, self.obj.animation_data.action_slot)

        self.assertIsNotNone(channelbag)
        self.assertEqual(len(channelbag.fcurves), 9)

        for fcurve in channelbag.fcurves:
            if fcurve.data_path == "location":
                self.assertAlmostEqual(fcurve.keyframe_points[-1].co.x, 15,
                                       6, f"Baking over an existing action should preserve all keys even those out of range ({fcurve.data_path})")
                self.assertEqual(len(fcurve.keyframe_points), 11, f"Unexpected key count on {fcurve.data_path}")
            else:
                self.assertAlmostEqual(fcurve.keyframe_points[-1].co.x, 9,
                                       6, f"Unexpected key y position on {fcurve.data_path}")
                self.assertEqual(len(fcurve.keyframe_points), 10, f"Unexpected key count on {fcurve.data_path}")

    def test_bake_object_multi_slot_to_new_action(self):
        obj2 = bpy.data.objects.new("obj2", None)
        bpy.context.scene.collection.objects.link(obj2)
        action = bpy.data.actions.new("test_action")
        self.obj.animation_data.action = action
        obj2.animation_data_create().action = action

        bpy.context.scene.frame_set(0)
        self.obj.location = (0, 0, 0)
        self.obj.keyframe_insert("location")
        obj2.location = (0, 1, 0)
        obj2.keyframe_insert("location")

        bpy.context.scene.frame_set(9)
        self.obj.location = (2, 0, 0)
        self.obj.keyframe_insert("location")
        obj2.location = (2, 1, 0)
        obj2.keyframe_insert("location")

        self.assertIsNotNone(self.obj.animation_data.action_slot)
        self.assertIsNotNone(obj2.animation_data.action_slot)
        self.assertNotEqual(self.obj.animation_data.action_slot, obj2.animation_data.action_slot)
        original_slot = obj2.animation_data.action_slot
        anim_utils.bake_action_objects([(obj2, None)], frames=range(0, 10), bake_options=OBJECT_BAKE_OPTIONS)

        self.assertNotEqual(action, obj2.animation_data.action, "Expected baking to result in a new action")
        baked_action = obj2.animation_data.action
        self.assertEqual(len(baked_action.slots), 1)
        self.assertEqual(original_slot.name_display, baked_action.slots[0].name_display)
        channelbag = anim_utils.action_get_channelbag_for_slot(baked_action, baked_action.slots[0])

        for fcurve in channelbag.fcurves:
            if fcurve.data_path != "location":
                continue
            # The keyframes should match the animation of obj2, not self.obj.
            if fcurve.array_index == 0:
                self.assertAlmostEqual(fcurve.keyframe_points[0].co.y, 0,
                                       6, f"Unexpected key y position on {fcurve.data_path}")
                self.assertAlmostEqual(fcurve.keyframe_points[-1].co.y, 2,
                                       6, f"Unexpected key y position on {fcurve.data_path}")
            elif fcurve.array_index == 1:
                self.assertAlmostEqual(fcurve.keyframe_points[0].co.y, 1,
                                       6, f"Unexpected key y position on {fcurve.data_path}")
                self.assertAlmostEqual(fcurve.keyframe_points[-1].co.y, 1,
                                       6, f"Unexpected key y position on {fcurve.data_path}")

    def test_bake_object_multi_slot_to_existing_action(self):
        obj2 = bpy.data.objects.new("obj2", None)
        bpy.context.scene.collection.objects.link(obj2)
        action = bpy.data.actions.new("test_action")
        self.obj.animation_data.action = action
        obj2.animation_data_create().action = action

        bpy.context.scene.frame_set(0)
        self.obj.location = (0, 0, 0)
        self.obj.keyframe_insert("location")
        obj2.location = (0, 1, 0)
        obj2.keyframe_insert("location")

        bpy.context.scene.frame_set(15)
        self.obj.location = (2, 0, 0)
        self.obj.keyframe_insert("location")
        obj2.location = (2, 1, 0)
        obj2.keyframe_insert("location")

        self.assertEqual(len(action.slots), 2)

        self.assertIsNotNone(self.obj.animation_data.action_slot)
        self.assertIsNotNone(obj2.animation_data.action_slot)

        anim_utils.bake_action_objects([(obj2, action)], frames=range(0, 10), bake_options=OBJECT_BAKE_OPTIONS)

        self.assertEqual(action, obj2.animation_data.action)
        self.assertEqual(len(action.slots), 2, "Didn't expect baking to create a new slot")
        self.assertNotEqual(obj2.animation_data.action_slot, self.obj.animation_data.action_slot)

        channelbag = anim_utils.action_get_channelbag_for_slot(action, obj2.animation_data.action_slot)

        self.assertIsNotNone(channelbag)
        self.assertEqual(len(channelbag.fcurves), 9)

        for fcurve in channelbag.fcurves:
            # The keyframes should match the animation of obj2, not self.obj.
            if fcurve.data_path == "location":
                self.assertAlmostEqual(fcurve.keyframe_points[-1].co.x, 15,
                                       6, f"Baking over an existing action should preserve all keys even those out of range ({fcurve.data_path})")
                self.assertEqual(len(fcurve.keyframe_points), 11, f"Unexpected key count on {fcurve.data_path}")
                if fcurve.array_index == 0:
                    self.assertAlmostEqual(fcurve.keyframe_points[-1].co.y, 2,
                                           6, f"Unexpected key y position on {fcurve.data_path}")
                elif fcurve.array_index == 1:
                    self.assertAlmostEqual(fcurve.keyframe_points[-1].co.y, 1,
                                           6, f"Unexpected key y position on {fcurve.data_path}")
            else:
                self.assertAlmostEqual(fcurve.keyframe_points[-1].co.x, 9,
                                       6, f"Unexpected key y position on {fcurve.data_path}")
                self.assertEqual(len(fcurve.keyframe_points), 10, f"Unexpected key count on {fcurve.data_path}")


def main():
    global args
    import argparse

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    parser = argparse.ArgumentParser()
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
