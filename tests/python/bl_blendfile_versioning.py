# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --python tests/python/bl_blendfile_versioning.py ..

# WARNING(@ideasman42): some blend files causes the tests to fail (seemingly) at random (on Linux & macOS at least).
# Take care when adding new files as they may break on other platforms, frequently but not on every execution.
#
# This needs to be investigated!

__all__ = (
    "main",
)

import os
import platform
import sys

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from bl_blendfile_utils import TestHelper


class TestBlendFileOpenLinkSaveAllTestFiles(TestHelper):

    def __init__(self, args):
        self.args = args
        # Some files are known broken currently for opening or linking.
        # They cannot be opened, or will generate some error (e.g. memleaks).
        # Each file in this list should either be the source of a bug report,
        # or removed from tests repo.
        self.excluded_open_link_paths = {
            # modifier_stack/explode_modifier.blend
            # BLI_assert failed: source/blender/blenlib/BLI_ordered_edge.hh:41, operator==(), at 'e1.v_low < e1.v_high'
            "explode_modifier.blend",

            # depsgraph/deg_anim_camera_dof_driving_material.blend
            # ERROR (bke.fcurve):
            # source/blender/blenkernel/intern/fcurve_driver.cc:188 dtar_get_prop_val:
            # Driver Evaluation Error: cannot resolve target for OBCamera ->
            # data.dof_distance
            "deg_anim_camera_dof_driving_material.blend",

            # depsgraph/deg_driver_shapekey_same_datablock.blend
            # Error: Not freed memory blocks: 4, total unfreed memory 0.000427 MB
            "deg_driver_shapekey_same_datablock.blend",

            # physics/fluidsim.blend
            # Error: Not freed memory blocks: 3, total unfreed memory 0.003548 MB
            "fluidsim.blend",

            # opengl/ram_glsl.blend
            # Error: Not freed memory blocks: 4, total unfreed memory 0.000427 MB
            "ram_glsl.blend",
        }

        # Directories to exclude relative to `./tests/files/`.
        self.excluded_open_link_dirs = ()

        # Some files are known broken currently on re-saving & re-opening.
        # Each file in this list should either be the source of a bug report,
        # or removed from tests repo.
        self.excluded_save_reload_paths = {
            # gameengine_bullet_softbody/softbody_constraints.blend
            # Error: on save,
            #        'Unable to pack file, source path '.../gameengine_bullet_softbody/marble_256.jpg' not found'
            "softbody_constraints.blend",

            # files/libraries_and_linking/library_test_scene.blend
            # Error: on save and/or reload, creates memleaks.
            "library_test_scene.blend",

            # files/libraries_and_linking/libraries/main_scene.blend
            # Error: on save and/or reload, creates memleaks.
            "main_scene.blend",

            # modeling/geometry_nodes/import/import_obj.blend
            # Error: on reload,
            #        "OBJParser: Cannot read from OBJ file:
            #         '/home/guest/blender/main/build_main_release/tests/blendfile_io/data_files/icosphere.obj'"
            "import_obj.blend",

            # grease_pencil/grease_pencil_paper_pig.blend
            # Error: on save and/or reload, creates memleaks.
            "grease_pencil_paper_pig.blend",

            # modeling/geometry_nodes/import/import_ply.blend
            # Error: on reload, 'read_ply_to_mesh: PLY Importer: icosphere: Invalid PLY header.'
            "import_ply.blend",

            # modeling/geometry_nodes/import/import_stl.blend
            # Error: on reload,
            #        'read_stl_file: Failed to open STL file:'...tests/blendfile_io/data_files/icosphere.stl'.'
            "import_stl.blend",
        }

        # Directories to exclude relative to `./tests/files/`.
        self.excluded_save_reload_dirs = ()

        # Some files are expected to be invalid.
        # This mapping stores filenames as keys, and expected error message as value.
        self.invalid_paths = {
            # animation/driver-object-eyes.blend
            # File generated from a big endian build of Blender.
            "driver-object-eyes.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),

            # modeling/faceselectmode.blend
            # File generated from a big endian build of Blender.
            "faceselectmode.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # modeling/weight-paint_test.blend
            # File generated from a big endian build of Blender.
            "weight-paint_test.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),

            # io_tests/blend_big_endian/1.62/glass.blend
            # File generated from a big endian build of Blender.
            "glass.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/1.62/room.blend
            # File generated from a big endian build of Blender.
            "room.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/1.69/s-mesh.blend
            # File generated from a big endian build of Blender.
            "s-mesh.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/1.70/escher.blend
            # File generated from a big endian build of Blender.
            "escher.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/1.98/egypt.blend
            # File generated from a big endian build of Blender.
            "egypt.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.25/FroggyPacked.blend
            # File generated from a big endian build of Blender.
            "FroggyPacked.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.30/CtrlObject.blend
            # File generated from a big endian build of Blender.
            "CtrlObject.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.30/demofile.blend
            # File generated from a big endian build of Blender.
            "demofile.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.30/dolphin.blend
            # File generated from a big endian build of Blender.
            "dolphin.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.30/mball.blend
            # File generated from a big endian build of Blender.
            "mball.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.30/motor9.blend
            # File generated from a big endian build of Blender.
            "motor9.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.30/relative.blend
            # File generated from a big endian build of Blender.
            "relative.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.31/Raptor.blend
            # File generated from a big endian build of Blender.
            "Raptor.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.31/allselect.blend
            # File generated from a big endian build of Blender.
            "allselect.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.31/arealight.blend
            # File generated from a big endian build of Blender.
            "arealight.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.31/hairball.blend
            # File generated from a big endian build of Blender.
            "hairball.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.31/luxo.blend
            # File generated from a big endian build of Blender.
            "luxo.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.31/monkey_cornelius.blend
            # File generated from a big endian build of Blender.
            "monkey_cornelius.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.31/refract_monkey.blend
            # File generated from a big endian build of Blender.
            "refract_monkey.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.31/robo.blend
            # File generated from a big endian build of Blender.
            "robo.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.34/flippedmatrixes.blend
            # File generated from a big endian build of Blender.
            "flippedmatrixes.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.34/lostride.blend
            # File generated from a big endian build of Blender.
            "lostride.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.34/tapercurve.blend
            # File generated from a big endian build of Blender.
            "tapercurve.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.36/pathdist.blend
            # File generated from a big endian build of Blender.
            "pathdist.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_big_endian/2.76/bird_sintel.blend
            # File generated from a big endian build of Blender.
            "bird_sintel.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
            # io_tests/blend_parsing/BHead4_big_endian.blend
            # File generated from a big endian build of Blender.
            "BHead4_big_endian.blend": (
                (OSError, RuntimeError),
                "created by a Big Endian version of Blender, support for these files has been removed in Blender 5.0"
            ),
        }

        assert all(p.endswith("/") for p in self.excluded_open_link_dirs)
        self.excluded_open_link_dirs = tuple(p.replace("/", os.sep) for p in self.excluded_open_link_dirs)
        assert all(p.endswith("/") for p in self.excluded_save_reload_dirs)
        self.excluded_save_reload_dirs = tuple(p.replace("/", os.sep) for p in self.excluded_save_reload_dirs)

        # Generate the slice of blendfile paths that this instance of the test should process.
        blendfile_paths = [p for p in self.iter_blendfiles_from_directory(self.args.src_test_dir)]
        # `os.scandir()` used by `iter_blendfiles_from_directory` does not
        # guarantee any form of order.
        blendfile_paths.sort()
        slice_indices = self.generate_slice_indices(len(blendfile_paths), self.args.slice_range, self.args.slice_index)
        self.blendfile_paths = blendfile_paths[slice_indices[0]:slice_indices[1]]

    @classmethod
    def iter_blendfiles_from_directory(cls, root_path):
        for dir_entry in os.scandir(root_path):
            if dir_entry.is_dir(follow_symlinks=False):
                yield from cls.iter_blendfiles_from_directory(dir_entry.path)
            elif dir_entry.is_file(follow_symlinks=False):
                if os.path.splitext(dir_entry.path)[1] == ".blend":
                    yield dir_entry.path

    @staticmethod
    def generate_slice_indices(total_len, slice_range, slice_index):
        slice_stride_base = total_len // slice_range
        slice_stride_remain = total_len % slice_range

        def gen_indices(i):
            return (
                (i * (slice_stride_base + 1))
                if i < slice_stride_remain else
                (slice_stride_remain * (slice_stride_base + 1)) + ((i - slice_stride_remain) * slice_stride_base)
            )
        slice_indices = [(gen_indices(i), gen_indices(i + 1)) for i in range(slice_range)]
        return slice_indices[slice_index]

    def skip_path_check(self, bfp, excluded_paths, excluded_dirs):
        if os.path.basename(bfp) in excluded_paths:
            return True
        if excluded_dirs:
            assert bfp.startswith(self.args.src_test_dir)
            bfp_relative = bfp[len(self.args.src_test_dir):].rstrip(os.sep)
            if bfp_relative.startswith(*excluded_dirs):
                return True
        return False

    def skip_open_link_path_check(self, bfp):
        return self.skip_path_check(bfp, self.excluded_open_link_paths, self.excluded_open_link_dirs)

    def skip_save_reload_path_check(self, bfp):
        return self.skip_path_check(bfp, self.excluded_save_reload_paths, self.excluded_save_reload_dirs)

    def invalid_path_exception_process(self, bfp, exception):
        expected_failure = self.invalid_paths.get(os.path.basename(bfp), None)
        if not expected_failure:
            raise exception
        # Check expected exception type(s).
        if not isinstance(exception, expected_failure[0]):
            raise exception
        # Check expected exception (partial) message.
        if expected_failure[1] not in str(exception):
            raise exception
        print(f"\tExpected failure: '{exception}'", flush=True)

    def save_reload(self, bfp, prefix):
        if self.skip_save_reload_path_check(bfp):
            return
        # Use a hash to deduplicate the few blendfiles that have a same name,
        # but a different path (e.g. currently, `flip_faces.blend`).
        tmp_save_path = os.path.join(self.args.output_dir, prefix + hex(hash(bfp)) + "_" + os.path.basename(bfp))
        if not self.args.is_quiet:
            print(f"Trying to save to {tmp_save_path}", flush=True)
        bpy.ops.wm.save_as_mainfile(filepath=tmp_save_path, compress=True)
        if not self.args.is_quiet:
            print(f"Trying to reload from {tmp_save_path}", flush=True)
        bpy.ops.wm.revert_mainfile()
        if not self.args.is_quiet:
            print(f"Removing {tmp_save_path}", flush=True)
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        os.remove(tmp_save_path)

    def test_open(self):
        for bfp in self.blendfile_paths:
            if self.skip_open_link_path_check(bfp):
                continue
            if not self.args.is_quiet:
                print(f"Trying to open {bfp}", flush=True)
            bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
            try:
                bpy.ops.wm.open_mainfile(filepath=bfp, load_ui=False)
                self.save_reload(bfp, "OPENED_")
            except BaseException as e:
                self.invalid_path_exception_process(bfp, e)

    def link_append(self, do_link):
        operation_name = "link" if do_link else "append"
        for bfp in self.blendfile_paths:
            if self.skip_open_link_path_check(bfp):
                continue
            bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
            try:
                with bpy.data.libraries.load(bfp, link=do_link) as (lib_in, lib_out):
                    if len(lib_in.collections):
                        if not self.args.is_quiet:
                            print(f"Trying to {operation_name} {bfp}/Collection/{lib_in.collections[0]}", flush=True)
                        lib_out.collections.append(lib_in.collections[0])
                    elif len(lib_in.objects):
                        if not self.args.is_quiet:
                            print(f"Trying to {operation_name} {bfp}/Object/{lib_in.objects[0]}", flush=True)
                        lib_out.objects.append(lib_in.objects[0])
                self.save_reload(bfp, f"{operation_name.upper()}_")
            except BaseException as e:
                self.invalid_path_exception_process(bfp, e)

    def test_link(self):
        self.link_append(do_link=True)

    def test_append(self):
        self.link_append(do_link=False)


TESTS = (
    TestBlendFileOpenLinkSaveAllTestFiles,
)


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    description = ("Test basic versioning and writing code by opening, linking from, saving and reloading"
                   "all blend files in `--src-test-dir` directory (typically the `tests/files` one).")
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "--src-test-dir",
        dest="src_test_dir",
        default="..",
        help="Root tests directory to search for blendfiles",
        required=False,
    )
    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        default=".",
        help="Where to output temp saved blendfiles",
        required=False,
    )

    parser.add_argument(
        "--quiet",
        dest="is_quiet",
        type=bool,
        default=False,
        help="Whether to quiet prints of all blendfile read/link attempts",
        required=False,
    )

    parser.add_argument(
        "--slice-range",
        dest="slice_range",
        type=int,
        default=1,
        help="How many instances of this test are launched in parallel, the list of available blendfiles is then sliced "
             "and each instance only processes the part matching its given `--slice-index`.",
        required=False,
    )
    parser.add_argument(
        "--slice-index",
        dest="slice_index",
        type=int,
        default=0,
        help="The index of the slice in blendfiles that this instance should process."
             "Should always be specified when `--slice-range` > 1",
        required=False,
    )

    return parser


def main():
    args = argparse_create().parse_args()

    assert args.slice_range > 0
    assert 0 <= args.slice_index < args.slice_range

    for Test in TESTS:
        Test(args).run_all_tests()


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    main()
