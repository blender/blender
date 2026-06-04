# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0
import unittest

# ./blender.bin --background --python tests/python/bl_blendfile_autosave.py -- --output-dir=/tmp/
import bpy
import os
import re
import sys
import tempfile

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


class TestBlendFileImageAutosave(TestHelper):
    def __init__(self, args):
        super().__init__(args)

    def setUpClass(self):
        self.ensure_path(self.args.output_dir)
        self.temp_dir = tempfile.TemporaryDirectory(dir=self.args.output_dir)

    def tearDownClass(self):
        self.temp_dir.cleanup()

    @staticmethod
    def find_autosave_path(base_dir, base_filename):
        pattern = re.compile("{}_[0-9]+_autosave.blend".format(base_filename))

        paths = os.listdir(base_dir)
        for path in paths:
            if pattern.match(path):
                return os.path.join(base_dir, path)

        return None

    @staticmethod
    def modify_image(image):
        width, height = image.size
        color = (1.0, 1.0, 1.0, 1.0)

        pixels = width * height
        for pixel in range(pixels):
            for channel in range(4):
                image.pixels[pixel * 4 + channel] = color[channel]

    def check_image(self, image, expected_color):
        width, height = image.size

        pixels = width * height

        actual_data = list(image.pixels[:])
        expected_data = [0.0] * pixels * 4
        for pixel in range(pixels):
            for channel in range(4):
                expected_data[pixel * 4 + channel] = expected_color[channel]

        self.assertEqual(actual_data, expected_data)

    def test_generated_image_restore(self):
        self.ensure_path(self.args.output_dir)
        with tempfile.TemporaryDirectory(dir=self.args.output_dir) as output_dir:
            bpy.context.preferences.filepaths.temporary_directory = output_dir
            output_path = os.path.join(output_dir, "blendfile_autosave_generated_image.blend")

            # Immediately create the file so we can ensure we're working on a
            # "generated" and not packed image with autosave
            bpy.ops.wm.save_as_mainfile(filepath=output_path, check_existing=False, compress=False)

            orig_image = bpy.data.images.new("GeneratedImage", 2, 2, alpha=True)
            orig_image.use_fake_user = True

            self.modify_image(orig_image)
            self.check_image(orig_image, (1.0, 1.0, 1.0, 1.0))

            bpy.ops.wm.save_auto_save()
            bpy.ops.wm.open_mainfile(filepath=output_path, load_ui=False)

            # File should not have image, only autosave should have image
            self.assertNotIn("GeneratedImage", bpy.data.images)

            autosave_path = self.find_autosave_path(output_dir, "blendfile_autosave_generated_image")
            self.assertTrue(autosave_path)
            self.assertNotEqual(output_path, autosave_path)
            retval = bpy.ops.wm.recover_auto_save(filepath=autosave_path)
            self.assertEqual(retval, {'FINISHED'})

            self.assertIn("GeneratedImage", bpy.data.images)
            self.check_image(bpy.data.images["GeneratedImage"], (1.0, 1.0, 1.0, 1.0))

            bpy.ops.wm.open_mainfile(filepath=output_path, load_ui=False)
            self.assertNotIn("GeneratedImage", bpy.data.images)

    def test_packed_image_restore(self):
        self.ensure_path(self.args.output_dir)
        with tempfile.TemporaryDirectory(dir=self.args.output_dir) as output_dir:
            bpy.context.preferences.filepaths.temporary_directory = output_dir

            input_blendfile_path = os.path.join(self.args.src_test_dir, "autosave_image_test.blend")
            bpy.ops.wm.open_mainfile(filepath=input_blendfile_path)

            output_path = os.path.join(output_dir, "blendfile_autosave_packed_image.blend")
            bpy.ops.wm.save_as_mainfile(filepath=output_path, check_existing=False, compress=False)

            self.modify_image(bpy.data.images["PackedImage"])
            self.check_image(bpy.data.images["PackedImage"], (1.0, 1.0, 1.0, 1.0))

            bpy.ops.wm.save_auto_save()
            bpy.ops.wm.open_mainfile(filepath=output_path, load_ui=False)

            self.assertIn("PackedImage", bpy.data.images)
            self.check_image(bpy.data.images["PackedImage"], (0.0, 0.0, 0.0, 1.0))

            autosave_path = self.find_autosave_path(output_dir, "blendfile_autosave_packed_image")
            self.assertTrue(autosave_path)
            self.assertNotEqual(output_path, autosave_path)
            retval = bpy.ops.wm.recover_auto_save(filepath=autosave_path)
            self.assertEqual(retval, {'FINISHED'})

            # Loading the autosave file should show the "temporary" changes
            self.assertIn("PackedImage", bpy.data.images)
            self.check_image(bpy.data.images["PackedImage"], (1.0, 1.0, 1.0, 1.0))

    def test_external_image_restore(self):
        self.ensure_path(self.args.output_dir)
        with tempfile.TemporaryDirectory(dir=self.args.output_dir) as output_dir:
            bpy.context.preferences.filepaths.temporary_directory = output_dir

            input_blendfile_path = os.path.join(self.args.src_test_dir, "autosave_image_test.blend")
            bpy.ops.wm.open_mainfile(filepath=input_blendfile_path)

            output_path = os.path.join(output_dir, "blendfile_autosave_external_image.blend")
            bpy.ops.wm.save_as_mainfile(filepath=output_path, check_existing=False, compress=False)

            self.modify_image(bpy.data.images["ExternalImage"])
            self.check_image(bpy.data.images["ExternalImage"], (1.0, 1.0, 1.0, 1.0))

            bpy.ops.wm.save_auto_save()
            bpy.ops.wm.open_mainfile(filepath=output_path, load_ui=False)

            self.assertIn("ExternalImage", bpy.data.images)
            self.check_image(bpy.data.images["ExternalImage"], (0.0, 0.0, 0.0, 1.0))

            autosave_path = self.find_autosave_path(output_dir, "blendfile_autosave_external_image")
            self.assertTrue(autosave_path)
            self.assertNotEqual(output_path, autosave_path)
            retval = bpy.ops.wm.recover_auto_save(filepath=autosave_path)
            self.assertEqual(retval, {'FINISHED'})

            # Loading the autosave file should show the "temporary" changes
            self.assertIn("ExternalImage", bpy.data.images)
            self.check_image(bpy.data.images["ExternalImage"], (1.0, 1.0, 1.0, 1.0))

            # Loading the actual file should show the original data
            bpy.data.images["ExternalImage"].reload()
            self.check_image(bpy.data.images["ExternalImage"], (0.0, 0.0, 0.0, 1.0))

        return


TESTS = (
    TestBlendFileAutosave,
    TestBlendFileImageAutosave
)


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    description = "Test basic IO of autosaving a .blend file."
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "--src-test-dir",
        dest="src_test_dir",
        help="Root tests directory to search for blendfiles",
        required=True,
    )
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
