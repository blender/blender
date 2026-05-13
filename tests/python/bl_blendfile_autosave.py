# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --python tests/python/bl_blendfile_autosave.py -- --output-dir=/tmp/
import bpy
import os
import re
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from bl_blendfile_utils import TestHelper


class TestBlendFileAutosave(TestHelper):
    def __init__(self, args):
        super().__init__(args)

    @staticmethod
    def find_autosave_path(base_dir, base_filename):
        pattern = re.compile("{}_[0-9]+_autosave.blend".format(base_filename))

        paths = os.listdir(base_dir)
        for path in paths:
            if pattern.match(path):
                return os.path.join(base_dir, path)

        return None

    def test_autosave_restore(self):
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        orig_mesh = bpy.data.meshes.new("OrigCubeMesh")
        orig_mesh.use_fake_user = True

        output_dir = self.args.output_dir
        self.ensure_path(output_dir)
        bpy.context.preferences.filepaths.temporary_directory = output_dir

        output_path = os.path.join(output_dir, "blendfile_autosave.blend")

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_path, check_existing=False, compress=False)

        new_mesh = bpy.data.meshes.new("NewCubeMesh")
        new_mesh.use_fake_user = True

        autosave_data = self.blender_data_to_tuple(bpy.data, "autosave_data")

        bpy.ops.wm.save_auto_save()
        bpy.ops.wm.open_mainfile(filepath=output_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        self.assertEqual(read_data, orig_data)
        self.assertNotEqual(read_data, autosave_data)
        self.assertIn("OrigCubeMesh", bpy.data.meshes)
        self.assertNotIn("NewCubeMesh", bpy.data.meshes)

        autosave_path = self.find_autosave_path(output_dir, "blendfile_autosave")
        self.assertTrue(autosave_path)
        self.assertNotEqual(output_path, autosave_path)

        retval = bpy.ops.wm.recover_auto_save(filepath=autosave_path)
        self.assertEqual(retval, {'FINISHED'})

        recover_data = self.blender_data_to_tuple(bpy.data, "recover_data")
        self.assertEqual(recover_data, autosave_data)
        self.assertIn("OrigCubeMesh", bpy.data.meshes)
        self.assertIn("NewCubeMesh", bpy.data.meshes)


TESTS = (
    TestBlendFileAutosave,
)


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    description = "Test basic IO of autosaving a .blend file."
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        help="Where to output temp saved files",
        required=True,
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
