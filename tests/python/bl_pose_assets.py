# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
blender -b --factory-startup --python tests/python/bl_pose_assets.py -- --testdir /path/to/tests/files/animation
"""

__all__ = (
    "main",
)

import unittest
import bpy
import pathlib
import sys
import tempfile
import os


_BONE_NAME_1 = "bone"
_BONE_NAME_2 = "bone_2"
_LIB_NAME = "unit_test"

_BBONE_VALUES = {
    f'pose.bones["{_BONE_NAME_1}"].bbone_curveinx': (0, ),
    f'pose.bones["{_BONE_NAME_1}"].bbone_curveoutx': (0, ),
    f'pose.bones["{_BONE_NAME_1}"].bbone_curveinz': (0, ),
    f'pose.bones["{_BONE_NAME_1}"].bbone_curveoutz': (0, ),
    f'pose.bones["{_BONE_NAME_1}"].bbone_rollin': (0, ),
    f'pose.bones["{_BONE_NAME_1}"].bbone_rollout': (0, ),
    f'pose.bones["{_BONE_NAME_1}"].bbone_scalein': (1, 1, 1),
    f'pose.bones["{_BONE_NAME_1}"].bbone_scaleout': (1, 1, 1),
    f'pose.bones["{_BONE_NAME_1}"].bbone_easein': (0, ),
    f'pose.bones["{_BONE_NAME_1}"].bbone_easeout': (0, ),
}


def _create_armature():
    armature = bpy.data.armatures.new("anim_armature")
    armature_obj = bpy.data.objects.new("anim_object", armature)
    bpy.context.scene.collection.objects.link(armature_obj)
    bpy.context.view_layer.objects.active = armature_obj
    armature_obj.select_set(True)

    bpy.ops.object.mode_set(mode='EDIT')
    edit_bone = armature.edit_bones.new(_BONE_NAME_1)
    edit_bone.head = (1, 0, 0)
    edit_bone = armature.edit_bones.new(_BONE_NAME_2)
    edit_bone.head = (1, 0, 0)

    return armature_obj


def _first_channelbag(action: bpy.types.Action) -> bpy.types.ActionChannelbag:
    """Return the first Channelbag of the Action."""
    assert isinstance(action, bpy.types.Action)
    return action.layers[0].strips[0].channelbags[0]


class CreateAssetTest(unittest.TestCase):

    _library_folder = None
    _library = None
    _armature_object = None

    def setUp(self):
        super().setUp()
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        self._armature_object = _create_armature()
        self._library_folder = tempfile.TemporaryDirectory("pose_asset_test")

        self._library = bpy.types.AssetLibraryCollection.new(name=_LIB_NAME, directory=self._library_folder.name)

        bpy.context.view_layer.objects.active = self._armature_object
        bpy.ops.object.mode_set(mode='POSE')
        self._armature_object.pose.bones[_BONE_NAME_1]["bool_test"] = True
        self._armature_object.pose.bones[_BONE_NAME_1]["float_test"] = 3.14
        self._armature_object.pose.bones[_BONE_NAME_1]["string_test"] = "foobar"

    def tearDown(self):
        super().tearDown()
        bpy.types.AssetLibraryCollection.remove(self._library)
        self._library = None
        self._library_folder.cleanup()

    def test_create_local_asset(self):
        self._armature_object.pose.bones[_BONE_NAME_1].location = (1, 1, 2)
        self._armature_object.pose.bones[_BONE_NAME_2].location = (-1, 0, 0)

        self._armature_object.pose.bones[_BONE_NAME_1].select = True
        self._armature_object.pose.bones[_BONE_NAME_2].select = False

        self._armature_object.pose.bones[_BONE_NAME_1].keyframe_insert('["bool_test"]')
        self._armature_object.pose.bones[_BONE_NAME_1].keyframe_insert('["float_test"]')

        # There is an action for the custom properties.
        self.assertEqual(len(bpy.data.actions), 1)
        bpy.ops.poselib.create_pose_asset(
            pose_name="local_asset",
            asset_library_reference='LOCAL',
            catalog_path="unit_test")

        self.assertEqual(len(bpy.data.actions), 2, "Local poses should be stored as actions")
        pose_action = bpy.data.actions[1]
        self.assertTrue(pose_action.asset_data is not None, "The created action should be marked as an asset")

        expected_pose_values = {
            f'pose.bones["{_BONE_NAME_1}"].location': (1, 1, 2),
            f'pose.bones["{_BONE_NAME_1}"].rotation_quaternion': (1, 0, 0, 0),
            f'pose.bones["{_BONE_NAME_1}"].scale': (1, 1, 1),

            f'pose.bones["{_BONE_NAME_1}"]["bool_test"]': (True, ),
            f'pose.bones["{_BONE_NAME_1}"]["float_test"]': (3.14, ),
            # string_test is not here because it should not be keyed.
        }
        expected_pose_values.update(_BBONE_VALUES)
        pose_channelbag = _first_channelbag(pose_action)
        self.assertEqual(len(pose_channelbag.fcurves), 26)
        for fcurve in pose_channelbag.fcurves:
            self.assertTrue(
                fcurve.data_path in expected_pose_values,
                "Only the selected bone should be in the pose asset")
            self.assertEqual(len(fcurve.keyframe_points), 1, "Only one key should have been created")
            self.assertEqual(fcurve.keyframe_points[0].co.x, 1, "Poses should be on the first frame")
            self.assertAlmostEqual(fcurve.keyframe_points[0].co.y,
                                   expected_pose_values[fcurve.data_path][fcurve.array_index], 4)

    def test_create_outside_asset(self):
        self._armature_object.pose.bones[_BONE_NAME_1].location = (1, 1, 2)
        self._armature_object.pose.bones[_BONE_NAME_2].location = (-1, 0, 0)

        self._armature_object.pose.bones[_BONE_NAME_1].select = True
        self._armature_object.pose.bones[_BONE_NAME_2].select = False

        self._armature_object.pose.bones[_BONE_NAME_1].keyframe_insert('["bool_test"]')
        self._armature_object.pose.bones[_BONE_NAME_1].keyframe_insert('["float_test"]')

        # There is an action for the custom properties.
        self.assertEqual(len(bpy.data.actions), 1)
        bpy.ops.poselib.create_pose_asset(
            pose_name="local_asset",
            asset_library_reference=_LIB_NAME,
            catalog_path="unit_test")

        self.assertEqual(len(bpy.data.actions), 1, "The asset should not have been created in this file")
        actions_folder = os.path.join(self._library.path, "Saved", "Actions")
        asset_files = os.listdir(actions_folder)
        self.assertEqual(len(asset_files),
                         1, "The pose asset file should have been created")

        with bpy.data.libraries.load(os.path.join(actions_folder, asset_files[0])) as (data_from, data_to):
            self.assertEqual(data_from.actions, ["local_asset"])
            data_to.actions = data_from.actions

        pose_action = data_to.actions[0]

        self.assertTrue(pose_action.asset_data is not None, "The created action should be marked as an asset")
        expected_pose_values = {
            f'pose.bones["{_BONE_NAME_1}"].location': (1, 1, 2),
            f'pose.bones["{_BONE_NAME_1}"].rotation_quaternion': (1, 0, 0, 0),
            f'pose.bones["{_BONE_NAME_1}"].scale': (1, 1, 1),
            f'pose.bones["{_BONE_NAME_1}"]["bool_test"]': (True, ),
            f'pose.bones["{_BONE_NAME_1}"]["float_test"]': (3.14, ),
            # string_test is not here because it should not be keyed.
        }
        expected_pose_values.update(_BBONE_VALUES)
        pose_channelbag = _first_channelbag(pose_action)
        self.assertEqual(len(pose_channelbag.fcurves), 26)
        for fcurve in pose_channelbag.fcurves:
            self.assertTrue(
                fcurve.data_path in expected_pose_values,
                "Only the selected bone should be in the pose asset")
            self.assertEqual(len(fcurve.keyframe_points), 1, "Only one key should have been created")
            self.assertEqual(fcurve.keyframe_points[0].co.x, 1, "Poses should be on the first frame")
            self.assertAlmostEqual(fcurve.keyframe_points[0].co.y,
                                   expected_pose_values[fcurve.data_path][fcurve.array_index], 4)

    def test_custom_properties_without_keys(self):
        # Custom properties without keys should not be added to the pose asset.
        self._armature_object.pose.bones[_BONE_NAME_1].location = (1, 1, 2)
        self._armature_object.pose.bones[_BONE_NAME_2].location = (-1, 0, 0)

        self._armature_object.pose.bones[_BONE_NAME_1].select = True
        self._armature_object.pose.bones[_BONE_NAME_2].select = False

        self.assertEqual(len(bpy.data.actions), 0)
        bpy.ops.poselib.create_pose_asset(
            pose_name="local_asset",
            asset_library_reference='LOCAL',
            catalog_path="unit_test")

        pose_action = bpy.data.actions[0]
        self.assertTrue(pose_action.asset_data is not None, "The created action should be marked as an asset")

        expected_pose_values = {
            f'pose.bones["{_BONE_NAME_1}"].location': (1, 1, 2),
            f'pose.bones["{_BONE_NAME_1}"].rotation_quaternion': (1, 0, 0, 0),
            f'pose.bones["{_BONE_NAME_1}"].scale': (1, 1, 1),

            # The custom properties are not keyed, thus they should not be in the pose asset.
        }
        expected_pose_values.update(_BBONE_VALUES)
        pose_channelbag = _first_channelbag(pose_action)
        self.assertEqual(len(pose_channelbag.fcurves), 24)
        for fcurve in pose_channelbag.fcurves:
            self.assertTrue(
                fcurve.data_path in expected_pose_values,
                "Only the selected bone should be in the pose asset")
            self.assertEqual(len(fcurve.keyframe_points), 1, "Only one key should have been created")
            self.assertEqual(fcurve.keyframe_points[0].co.x, 1, "Poses should be on the first frame")
            self.assertAlmostEqual(fcurve.keyframe_points[0].co.y,
                                   expected_pose_values[fcurve.data_path][fcurve.array_index], 4)


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
