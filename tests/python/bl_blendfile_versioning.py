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

            # invalid_blendfiles/invalid_pointcache_totpoint_overflow.blend
            # Allocates a lot of memory due to invalid file, causing test failures.
            # on Windows ARM64 CI.
            "invalid_pointcache_totpoint_overflow.blend",
        }

        # Directories to exclude relative to `./tests/files/`.
        self.excluded_dirs = (
            # invalid_blendfiles/
            # Files saved by newer Blender versions, which leak memory on reading (asset shelf
            # regions, strips, implicitly shared data). Tested separately by
            # `blendfile_versioning_invalid`, without failing on memory leaks.
            "invalid_blendfiles/",
        )

        # Some files are known broken currently for linking and appending only.
        self.excluded_link_append_paths = {
            # invalid_blendfiles/invalid_struct_size_boid_rule.blend
            # invalid_blendfiles/invalid_struct_size_modifier.blend
            # Linking from a library that fails to read leaves data-blocks with unresolved pointers
            # in the main database (crash).
            "invalid_struct_size_boid_rule.blend",
            "invalid_struct_size_modifier.blend",
        }

        # Some files are expected to be invalid.
        # This mapping stores filenames as keys, and expected error message as value.
        self.invalid_paths = {
            # invalid_blendfiles/invalid_sdna_struct_size.blend
            # A struct's DNA size no longer matches the sum of its members.
            "invalid_sdna_struct_size.blend": (
                (OSError, RuntimeError),
                "Invalid struct size in SDNA file"
            ),
            # invalid_blendfiles/invalid_block_count.blend
            # A file-block declares an out-of-range array element count.
            "invalid_block_count.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, invalid block count"
            ),
            # invalid_blendfiles/invalid_block_struct_index.blend
            # A file-block references an out-of-range SDNA struct index.
            "invalid_block_struct_index.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, invalid block struct index"
            ),
            # invalid_blendfiles/invalid_global_block.blend
            # The required global block references an out-of-range SDNA struct index.
            "invalid_global_block.blend": (
                (OSError, RuntimeError),
                "is corrupt, unable to read"
            ),
            # invalid_blendfiles/invalid_window_workspace_hook.blend
            # A window's sub-block references an out-of-range SDNA struct index.
            "invalid_window_workspace_hook.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, invalid block struct index"
            ),
            # invalid_blendfiles/invalid_node_tree_failed_read.blend
            # A block after a node tree references an out-of-range SDNA struct index.
            "invalid_node_tree_failed_read.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, invalid block struct index"
            ),
            # invalid_blendfiles/invalid_glob_short.blend
            # The global block is shorter than its sub-version string.
            "invalid_glob_short.blend": (
                (OSError, RuntimeError),
                "is corrupt, unable to read"
            ),
            # invalid_blendfiles/invalid_id_short.blend
            # An ID block is too short for the ID name.
            "invalid_id_short.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, invalid block count"
            ),
            # invalid_blendfiles/invalid_id_small.blend
            # An ID block is smaller than its struct.
            "invalid_id_small.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, invalid block count"
            ),
            # invalid_blendfiles/invalid_struct_count_zero.blend
            # A block has zero struct count.
            "invalid_struct_count_zero.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, invalid block count"
            ),
            # invalid_blendfiles/invalid_glob_small.blend
            # The global block has a struct type smaller than FileGlobal.
            "invalid_glob_small.blend": (
                (OSError, RuntimeError),
                "is corrupt, unable to read"
            ),
            # invalid_blendfiles/invalid_id_struct_type.blend
            # An ID block has a struct type smaller than its ID type.
            "invalid_id_struct_type.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, block too small for its type"
            ),
            # invalid_blendfiles/invalid_library_id_type.blend
            # A library block has the ID name of a larger type.
            "invalid_library_id_type.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, block too small for its type"
            ),
            # invalid_blendfiles/invalid_library_missing.blend
            # Link placeholders without a library block.
            "invalid_library_missing.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, missing library"
            ),
            # invalid_blendfiles/invalid_library_as_text.blend
            # Link placeholders after a library block renamed to a text.
            "invalid_library_as_text.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, missing library"
            ),
            # invalid_blendfiles/invalid_library_filepath_unterminated.blend
            # Library filepath without null terminator.
            "invalid_library_filepath_unterminated.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, invalid library file path"
            ),
            # invalid_blendfiles/invalid_library_filepath_unterminated_listing.blend
            # Library filepath overwriting the library flags, marking it as archive library.
            "invalid_library_filepath_unterminated_listing.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, invalid library file path"
            ),
            # invalid_blendfiles/invalid_preview_rect_short.blend
            # A preview pixel block smaller than the preview size.
            "invalid_preview_rect_short.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, unexpected data size"
            ),
            # invalid_blendfiles/invalid_struct_size_*.blend
            # A block with a type member is too small for the struct of its type.
            "invalid_struct_size_boid_rule.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, unexpected data size"
            ),
            "invalid_struct_size_grease_pencil_drawing.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, unexpected data size"
            ),
            "invalid_struct_size_grease_pencil_layer.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, unexpected data size"
            ),
            "invalid_struct_size_modifier.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, unexpected data size"
            ),
            "invalid_struct_size_shader_fx.blend": (
                (OSError, RuntimeError),
                "Corrupt .blend file, unexpected data size"
            ),
        }

        assert all(p.endswith("/") for p in self.excluded_dirs)
        self.excluded_dirs = tuple(p.replace("/", os.sep) for p in self.excluded_dirs)

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

    def skip_path_check(self, bfp):
        if os.path.basename(bfp) in self.excluded_paths:
            return True
        if self.excluded_dirs:
            assert bfp.startswith(self.args.src_test_dir)
            bfp_relative = bfp[len(self.args.src_test_dir):].rstrip(os.sep)
            if bfp_relative.startswith(*self.excluded_dirs):
                return True
        return False

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

    def test_open(self):
        for bfp in self.blendfile_paths:
            if self.skip_path_check(bfp):
                continue
            if not self.args.is_quiet:
                print(f"Trying to open {bfp}", flush=True)
            bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
            try:
                bpy.ops.wm.open_mainfile(filepath=bfp, load_ui=False)
            except BaseException as e:
                self.invalid_path_exception_process(bfp, e)

    def link_append(self, do_link):
        operation_name = "link" if do_link else "append"
        for bfp in self.blendfile_paths:
            if self.skip_path_check(bfp):
                continue
            if os.path.basename(bfp) in self.excluded_link_append_paths:
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
            except BaseException as e:
                self.invalid_path_exception_process(bfp, e)

    def test_link(self):
        self.link_append(do_link=True)

    def test_append(self):
        self.link_append(do_link=False)


TESTS = (
    TestBlendFileOpenAllTestFiles,
)


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    description = ("Test basic versioning code by opening all blend files "
                   "in `tests/files` directory.")
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "--src-test-dir",
        dest="src_test_dir",
        default="..",
        help="Root tests directory to search for blendfiles",
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
