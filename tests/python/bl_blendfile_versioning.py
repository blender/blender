# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background -noaudio --python tests/python/bl_blendfile_versioning.py ..
import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from bl_blendfile_utils import TestHelper


class TestBlendFileOpenAllTestFiles(TestHelper):

    def __init__(self, args):
        self.args = args
        # Some files are known broken currently.
        # Each file in this list should either be the source of a bug report,
        # or removed from tests repo.
        self.excluded_paths = {
            # tests/modifier_stack/explode_modifier.blend
            # BLI_assert failed: source/blender/blenlib/BLI_ordered_edge.hh:41, operator==(), at 'e1.v_low < e1.v_high'
            "explode_modifier.blend",

            # tests/depsgraph/deg_anim_camera_dof_driving_material.blend
            # ERROR (bke.fcurve):
            # source/blender/blenkernel/intern/fcurve_driver.cc:188 dtar_get_prop_val:
            # Driver Evaluation Error: cannot resolve target for OBCamera ->
            # data.dof_distance
            "deg_anim_camera_dof_driving_material.blend",

            # tests/depsgraph/deg_driver_shapekey_same_datablock.blend
            # Error: Not freed memory blocks: 4, total unfreed memory 0.000427 MB
            "deg_driver_shapekey_same_datablock.blend",

            # tests/physics/fluidsim.blend
            # Error: Not freed memory blocks: 3, total unfreed memory 0.003548 MB
            "fluidsim.blend",

            # tests/opengl/ram_glsl.blend
            # Error: Not freed memory blocks: 4, total unfreed memory 0.000427 MB
            "ram_glsl.blend",
        }

    @classmethod
    def iter_blendfiles_from_directory(cls, root_path):
        for dir_entry in os.scandir(root_path):
            if dir_entry.is_dir(follow_symlinks=False):
                yield from cls.iter_blendfiles_from_directory(dir_entry.path)
            elif dir_entry.is_file(follow_symlinks=False):
                if os.path.splitext(dir_entry.path)[1] == ".blend":
                    yield dir_entry.path

    def test_open(self):
        import subprocess
        blendfile_paths = [p for p in self.iter_blendfiles_from_directory(self.args.src_test_dir)]
        # `os.scandir()` used by `iter_blendfiles_from_directory` does not
        # guarantee any form of order.
        blendfile_paths.sort()
        for bfp in blendfile_paths:
            if os.path.basename(bfp) in self.excluded_paths:
                continue
            bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
            bpy.ops.wm.open_mainfile(filepath=bfp, load_ui=False)


TESTS = (
    TestBlendFileOpenAllTestFiles,
)


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    description = ("Test basic versioning code by opening all blend files "
                   "in `lib/tests` directory.")
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "--src-test-dir",
        dest="src_test_dir",
        default="..",
        help="Root tests directory to search for blendfiles",
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
