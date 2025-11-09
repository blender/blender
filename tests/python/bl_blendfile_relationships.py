# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

"""
blender -b -P tests/python/bl_blendfile_relationships.py --src-test-dir tests/files --output-dir /tmp/blendfile_io/
"""
__all__ = (
    "main",
)

import bpy
import os
import sys
from pathlib import Path

from bpy.path import native_pathsep

_my_dir = Path(__file__).resolve().parent
sys.path.append(str(_my_dir))

from bl_blendfile_utils import TestBlendLibLinkHelper, TestHelper


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
            self.assertEqual(user_map[k], v, msg=f"ID {k.name} has unexpected user map")

        user_map = bpy.data.user_map(subset=[bpy.data.objects[0], bpy.data.meshes[0]])
        expected_map = {
            bpy.data.meshes[0]: {bpy.data.objects[0]},
            bpy.data.objects[0]: {bpy.data.scenes[0],
                                  bpy.data.collections[0]},
        }
        for k, v in expected_map.items():
            self.assertIn(k, user_map)
            self.assertEqual(user_map[k], v, msg=f"ID {k.name} has unexpected user map")
        user_map = bpy.data.user_map(key_types={'OBJECT', 'MESH'})
        for k, v in expected_map.items():
            self.assertIn(k, user_map)
            self.assertEqual(user_map[k], v, msg=f"ID {k.name} has unexpected user map")

        user_map = bpy.data.user_map(value_types={'SCENE'})
        expected_map = {
            bpy.data.collections[0]: {bpy.data.scenes[0]},
            bpy.data.objects[0]: {bpy.data.scenes[0]},
        }
        for k, v in expected_map.items():
            self.assertIn(k, user_map)
            self.assertEqual(user_map[k], v, msg=f"ID {k.name} has unexpected user map")

        # Test handling of invalid parameters
        self.assertRaises(ValueError, bpy.data.user_map, value_types={'FOOBAR'})
        self.assertRaises(TypeError, bpy.data.user_map, subset=[bpy.data.objects[0], bpy.data.meshes[0], "FooBar"])


class TestBlendFilePathMap(TestBlendLibLinkHelper):

    def __init__(self, args):
        super().__init__(args)

    def test_file_path_map(self):
        def abspaths(file_path_map):
            return {k: {os.path.normpath(bpy.path.abspath(p)) for p in v}
                    for k, v in file_path_map.items()}

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

        blendlib_path = os.path.normpath(bpy.path.abspath(bpy.data.materials[0].library.filepath))
        image_path = os.path.join(native_pathsep(self.args.src_test_dir),
                                  native_pathsep('imbuf_io/reference/jpeg-rgb-90__from__rgba08.jpg'))

        file_path_map = abspaths(bpy.data.file_path_map())
        # Note: Workspaces and screens are ignored here.
        expected_map = {
            bpy.data.images[0]: {image_path},
            bpy.data.materials[0]: set(),
            bpy.data.scenes[0]: set(),
            bpy.data.collections[0]: set(),
            bpy.data.libraries[0]: {blendlib_path},
            bpy.data.meshes[0]: set(),
            bpy.data.objects[0]: set(),
            bpy.data.window_managers[0]: set(),
        }
        for k, v in expected_map.items():
            self.assertIn(k, file_path_map)
            self.assertEqual(file_path_map[k], v, msg=f"ID {k.name} has unexpected filepath map")

        file_path_map = abspaths(bpy.data.file_path_map(include_libraries=True))
        # Note: Workspaces and screens are ignored here.
        expected_map = {
            bpy.data.images[0]: {image_path, blendlib_path},
            bpy.data.materials[0]: {blendlib_path},
            bpy.data.scenes[0]: set(),
            bpy.data.collections[0]: set(),
            bpy.data.libraries[0]: {blendlib_path},
            bpy.data.meshes[0]: set(),
            bpy.data.objects[0]: set(),
            bpy.data.window_managers[0]: set(),
        }
        for k, v in expected_map.items():
            self.assertIn(k, file_path_map)
            self.assertEqual(file_path_map[k], v, msg=f"ID {k.name} has unexpected filepath map")

        file_path_map = abspaths(bpy.data.file_path_map(subset=[bpy.data.images[0], bpy.data.materials[0]]))
        expected_map = {
            bpy.data.images[0]: {image_path},
            bpy.data.materials[0]: set(),
        }
        for k, v in expected_map.items():
            self.assertIn(k, file_path_map)
            self.assertEqual(file_path_map[k], v, msg=f"ID {k.name} has unexpected filepath map")
        partial_map = abspaths(bpy.data.file_path_map(key_types={'IMAGE', 'MATERIAL'}))
        for k, v in expected_map.items():
            self.assertIn(k, file_path_map)
            self.assertEqual(file_path_map[k], v, msg=f"ID {k.name} has unexpected filepath map")

        # Test handling of invalid parameters
        self.assertRaises(ValueError, bpy.data.file_path_map, key_types={'FOOBAR'})
        self.assertRaises(TypeError, bpy.data.file_path_map, subset=[bpy.data.objects[0], bpy.data.images[0], "FooBar"])


class TestBlendFilePathForeach(TestHelper):
    testdir: Path

    def setUp(self) -> None:
        super().setUp()

        # File paths can get long, and thus also so can diffs when things go wrong.
        self.maxDiff = 10240
        self.testdir = Path(self.args.src_test_dir) / "libraries_and_linking"

        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "library_test_scene.blend"))

        # Name the library data-blocks after their filename, so that they can be reliably identified later.
        # Otherwise we're stuck with 'Lib', 'Lib.001', etc.
        for lib_id in bpy.data.libraries[:]:
            abspath = Path(str(bpy.path.abspath(lib_id.filepath)))
            lib_id.name = abspath.stem

    def test_without_args(self) -> None:
        """Test file_path_foreach() without any arguments, it should report everything but packed files."""

        visited_paths = self._file_path_foreach()
        self.assertEqual({
            (bpy.data.libraries['direct_linked_A'], self.testdir / "libraries/direct_linked_A.blend"),
            (bpy.data.libraries['direct_linked_B'], self.testdir / "libraries/direct_linked_B.blend"),
            (bpy.data.libraries['indirect_datablocks'], self.testdir / "libraries/indirect_datablocks.blend"),
        }, visited_paths)

    def test_with_nondefault_flag(self) -> None:
        """Test file_path_foreach() with a non-default flag.

        If any non-default flag works, it's enough as a test to assume the flags
        are parsed and passed to the C++ code correctly. There is no need to
        exhaustively test all the flags here.
        """

        # The default value for `flags` is {'SKIP_PACKED',
        # 'SKIP_WEAK_REFERENCES'}, so to also see the packed files, only pass
        # `SKIP_WEAK_REFERENCES`.
        visited_paths = self._file_path_foreach(flags={'SKIP_WEAK_REFERENCES'})
        self.assertEqual({
            (bpy.data.libraries['direct_linked_A'], self.testdir / "libraries/direct_linked_A.blend"),
            (bpy.data.libraries['direct_linked_B'], self.testdir / "libraries/direct_linked_B.blend"),
            (bpy.data.libraries['indirect_datablocks'], self.testdir / "libraries/indirect_datablocks.blend"),
            (bpy.data.images['pack.png'], self.testdir / "libraries/pack.png"),
        }, visited_paths, "testing without SKIP_PACKED")

    def test_filepath_rewriting(self) -> None:
        # Store the pre-modification value, as its use of (back)slashes is platform-dependent.
        image_filepath_before = str(bpy.data.images['pack.png'].filepath)

        def visit_path_fn(owner_id: bpy.types.ID, path: str, _meta: None) -> str | None:
            return "//{}-rewritten.blend".format(owner_id.name)
        bpy.data.file_path_foreach(visit_path_fn)

        libs = bpy.data.libraries
        self.assertEqual(libs['direct_linked_A'].filepath, "//direct_linked_A-rewritten.blend")
        self.assertEqual(libs['direct_linked_B'].filepath, "//direct_linked_B-rewritten.blend")
        self.assertEqual(libs['indirect_datablocks'].filepath, "//indirect_datablocks-rewritten.blend")
        self.assertEqual(
            bpy.data.images['pack.png'].filepath,
            image_filepath_before,
            "Packed file should not have changed")

    def test_exception_passing(self) -> None:
        """Python exceptions in the callback function should be raised by file_path_foreach()."""
        # Any Python exception should work, not just built-in ones.
        class CustomException(Exception):
            pass

        def visit_path_fn(_owner_id: bpy.types.ID, _path: str, meta: None) -> str | None:
            raise CustomException("arg0", 1, "arg2")

        try:
            bpy.data.file_path_foreach(visit_path_fn)
        except CustomException as ex:
            self.assertEqual(("arg0", 1, "arg2"), ex.args, "Parameters passed to the exception should be retained")
        else:
            self.fail("Expected exception not thrown")

    def test_meta_parameter(self) -> None:
        def visit_path_fn(_owner_id: bpy.types.ID, _path: str, meta: None) -> str | None:
            # This is proven to work by the `test_exception_passing()` test above.
            self.assertIsNone(
                meta,
                "The meta parameter is expected to be None; this test is expected to fail "
                "once the metadata feature is actually getting implemented, and should then "
                "be replaced with a proper test.")
        bpy.data.file_path_foreach(visit_path_fn)

    @staticmethod
    def _file_path_foreach(
        subtypes: set[str] | None = None,
        visit_keys: set[str] | None = None,
        flags: set[str] | None = None,
    ) -> set[tuple[bpy.types.ID, Path]]:
        """Call bpy.data.file_path_foreach(), returning the visited paths as set of (datablock, path) tuples.

        A set is used because the order of visiting is not relevant.
        """
        visisted_paths: set[tuple[bpy.types.ID, Path]] = set()

        def visit_path_fn(owner_id: bpy.types.ID, path: str, _meta: None) -> str | None:
            abspath = Path(str(bpy.path.abspath(path, library=owner_id.library)))
            visisted_paths.add((owner_id, abspath))

        # Dynamically build the keyword arguments, because the None values are not
        # valid, and should be encoded as not having the kwarg.
        kwargs = {}
        if subtypes is not None:
            kwargs['subtypes'] = subtypes
        if visit_keys is not None:
            kwargs['visit_keys'] = visit_keys
        if flags is not None:
            kwargs['flags'] = flags

        bpy.data.file_path_foreach(visit_path_fn, **kwargs)

        return visisted_paths


TESTS = (
    TestBlendUserMap,
    TestBlendFilePathMap,
    TestBlendFilePathForeach,
)


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    description = "Test basic relationship info of loaded data."
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
