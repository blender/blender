# Apache License, Version 2.0

# ./blender.bin --background -noaudio --python tests/python/bl_blendfile_liblink.py
import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from bl_blendfile_utils import TestHelper


class TestBlendLibLinkSaveLoadBasic(TestHelper):

    def __init__(self, args):
        self.args = args

    def test_link_save_load(self):
        bpy.ops.wm.read_factory_settings()
        me = bpy.data.meshes.new("LibMesh")
        me.use_fake_user = True

        output_dir = self.args.output_dir
        self.ensure_path(output_dir)
        output_path = os.path.join(output_dir, "blendlib.blend")

        bpy.ops.wm.save_as_mainfile(filepath=output_path, check_existing=False, compress=False)

        bpy.ops.wm.read_factory_settings()
        bpy.data.orphans_purge()

        link_dir = os.path.join(output_path, "Mesh")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh")

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        output_path = os.path.join(output_dir, "blendfile.blend")
        bpy.ops.wm.save_as_mainfile(filepath=output_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        assert(orig_data == read_data)



TESTS = (
    TestBlendLibLinkSaveLoadBasic,
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
