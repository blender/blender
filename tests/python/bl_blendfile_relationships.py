# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

"""
./blender.bin --background --python tests/python/bl_blendfile_liblink.py
"""
__all__ = (
    "main",
)

import bpy
import os
import sys

from bpy.path import native_pathsep

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from bl_blendfile_utils import TestBlendLibLinkHelper


class TestBlendUserMap(TestBlendLibLinkHelper):

    def __init__(self, args):
        super().__init__(args)

    def test_user_map(self):
        output_dir = self.args.output_dir
        output_blendfile_path = self.init_lib_data_indirect_lib()

        # Simple link of a single ObData.
        self.reset_blender()

        bpy.ops.wm.open_mainfile(filepath=output_blendfile_path)

        self.assertEqual(len(bpy.data.images), 1)
        self.assertIsNotNone(bpy.data.images[0].library)
        self.assertEqual(len(bpy.data.materials), 1)
        self.assertIsNotNone(bpy.data.materials[0].library)
        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)
        self.assertEqual(len(bpy.data.collections), 1)

        user_map = bpy.data.user_map()
        # Note: Workspaces and screens are ignored here.
        expected_map = {
            bpy.data.images[0]: {bpy.data.materials[0]},
            bpy.data.materials[0]: {bpy.data.meshes[0]},
            bpy.data.scenes[0]: {bpy.data.window_managers[0]},
            bpy.data.collections[0]: {bpy.data.scenes[0]},
            bpy.data.libraries[0]: set(),
            bpy.data.meshes[0]: {bpy.data.objects[0]},
            bpy.data.objects[0]: {bpy.data.scenes[0],
                                  bpy.data.collections[0]},
            bpy.data.window_managers[0]: set(),
        }
        for k, v in expected_map.items():
            self.assertIn(k, user_map)
            self.assertEqual(user_map[k], v)

        user_map = bpy.data.user_map(subset=[bpy.data.objects[0], bpy.data.meshes[0]])
        expected_map = {
            bpy.data.meshes[0]: {bpy.data.objects[0]},
            bpy.data.objects[0]: {bpy.data.scenes[0],
                                  bpy.data.collections[0]},
        }
        for k, v in expected_map.items():
            self.assertIn(k, user_map)
            self.assertEqual(user_map[k], v)
        user_map = bpy.data.user_map(key_types={'OBJECT', 'MESH'})
        for k, v in expected_map.items():
            self.assertIn(k, user_map)
            self.assertEqual(user_map[k], v)

        user_map = bpy.data.user_map(value_types={'SCENE'})
        expected_map = {
            bpy.data.collections[0]: {bpy.data.scenes[0]},
            bpy.data.objects[0]: {bpy.data.scenes[0]},
        }
        for k, v in expected_map.items():
            self.assertIn(k, user_map)
            self.assertEqual(user_map[k], v)

        # Test handling of invalid parameters
        self.assertRaises(ValueError, bpy.data.user_map, value_types={'FOOBAR'})
        self.assertRaises(TypeError, bpy.data.user_map, subset=[bpy.data.objects[0], bpy.data.meshes[0], "FooBar"])


TESTS = (
    TestBlendUserMap,
)


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    description = "Test basic realtionship info of loaded data."
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "--src-test-dir",
        dest="src_test_dir",
        default=".",
        help="Where to find test/data root directory",
        required=True,
    )
    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        default=".",
        help="Where to output temp saved blendfiles",
        required=False,
    )

    return parser


def main():
    args = argparse_create().parse_args()

    # Don't write thumbnails into the home directory.
    bpy.context.preferences.filepaths.file_preview_type = 'NONE'

    for Test in TESTS:
        Test(args).run_all_tests()


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    main()
