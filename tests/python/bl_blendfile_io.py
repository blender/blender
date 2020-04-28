# Apache License, Version 2.0

# ./blender.bin --background -noaudio --python tests/python/bl_blendfile_io.py
import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from bl_blendfile_utils import TestHelper


class TestBlendFileSaveLoadBasic(TestHelper):

    def __init__(self, args):
        self.args = args

    def test_save_load(self):
        bpy.ops.wm.read_factory_settings()
        bpy.data.meshes.new("OrphanedMesh")

        output_dir = self.args.output_dir
        self.ensure_path(output_dir)
        output_path = os.path.join(output_dir, "blendfile.blend")

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data 1")

        bpy.ops.wm.save_as_mainfile(filepath=output_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data 1")

        # We have orphaned data, which should be removed by file reading, so there should not be equality here.
        assert(orig_data != read_data)

        bpy.data.orphans_purge()

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data 2")

        bpy.ops.wm.save_as_mainfile(filepath=output_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data 2")

        assert(orig_data == read_data)



TESTS = (
    TestBlendFileSaveLoadBasic,
    )


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    description = "Test basic IO of blend file."
    parser = argparse.ArgumentParser(description=description)
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

    for Test in TESTS:
        Test(args).run_all_tests()


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    main()
