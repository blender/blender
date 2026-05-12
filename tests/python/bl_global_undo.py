# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --python tests/python/bl_global_undo.py ..

__all__ = (
    "main",
)

import contextlib
import io
import os
import platform
import sys

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from bl_blendfile_utils import TestBlendLibLinkHelper


NO_UNDO_ID_TYPES = {bpy.types.Brush, bpy.types.WindowManager, bpy.types.WorkSpace, bpy.types.Screen}


class IDStaticRepresentation:
    """
    A fully Blender-independant representation of a data-block.
    """

    def __init__(self, id_data, full=False):
        self.id_type = id_data.id_type
        self.name = id_data.name
        self.library = id_data.library.filepath if id_data.library else ""

        self.session_uid = id_data.session_uid
        self.pointer = id_data.as_pointer()

        self.content = self.extract_content(id_data) if full else {}

    @staticmethod
    def extract_content(id_data):
        """
        Generate a key-value (dict) representation of ID content, using RNA self-introspection mechanism.
        """
        # TODO.
        return {}

    def as_invariant_key(self):
        """Key value that should remain stable and be unique to an ID, even across e.g. blendfile reload."""
        return (self.id_type, self.name, self.library)

    def as_session_key(self):
        """
        Key value that should remain stable and be unique to an ID, during a single session.
        Not stable across e.g. blendfile reload.
        """
        return self.session_uid

    def __eq__(self, other):
        if self.id_type != other.id_type:
            return False
        if self.name != other.name:
            return False
        if self.library != other.library:
            return False
        if self.content != other.content:
            # Only consider content as different if it is defined in both ID representations.
            if self.content and other.content:
                return False
        return True

    def __hash__(self):
        return hash(self.as_invariant_key())


class TestGlobalUndo(TestBlendLibLinkHelper):

    def __init__(self, args):
        super().__init__(args)

    def link(self, lib_filepath, collections_names=[]):
        operation_name = "link"
        with bpy.data.libraries.load(lib_filepath, link=True) as (lib_in, lib_out):
            for cn in collections_names:
                lib_out.collections.append(lib_in.collections[cn])

    @staticmethod
    def get_all_ids():
        return {IDStaticRepresentation(id_data) for id_data in bpy.data.all_ids}

    def test_global_undo_empty(self):
        self.reset_blender()

        all_ids = self.get_all_ids()

        # NOTE: The two undo pushes are necessary to be able to undo, since the first undo push creates the
        # initial state for memfile undo (it is not initialized by default in background mode).
        bpy.ops.ed.undo_push()
        bpy.ops.ed.undo_push()
        bpy.ops.ed.undo_push()

        # Here all IDs remain present at all time, so their address themselves should remain unchanged.

        bpy.ops.ed.undo()
        self.assertEqual({id_data.pointer: id_data for id_data in all_ids},
                         {id_data.pointer: id_data for id_data in self.get_all_ids()})
        bpy.ops.ed.undo()
        self.assertEqual({id_data.pointer: id_data for id_data in all_ids},
                         {id_data.pointer: id_data for id_data in self.get_all_ids()})

        bpy.ops.ed.redo()
        self.assertEqual({id_data.pointer: id_data for id_data in all_ids},
                         {id_data.pointer: id_data for id_data in self.get_all_ids()})
        bpy.ops.ed.redo()
        self.assertEqual({id_data.pointer: id_data for id_data in all_ids},
                         {id_data.pointer: id_data for id_data in self.get_all_ids()})

        bpy.ops.ed.undo_history(item=0)
        self.assertEqual({id_data.pointer: id_data for id_data in all_ids},
                         {id_data.pointer: id_data for id_data in self.get_all_ids()})

        bpy.ops.ed.undo_history(item=2)
        self.assertEqual({id_data.pointer: id_data for id_data in all_ids},
                         {id_data.pointer: id_data for id_data in self.get_all_ids()})

    def test_global_undo_local_data(self):
        self.reset_blender()

        all_ids_init = self.get_all_ids()

        # NOTE: The two undo pushes are necessary to be able to undo, since the first undo push creates the
        # initial state for memfile undo (it is not initialized by default in background mode).
        bpy.ops.ed.undo_push()

        self.gen_library_data_()
        object_location_init = tuple(bpy.data.objects[0].location)
        bpy.ops.ed.undo_push()
        bpy.data.objects[0].location.x += 1.0
        object_location_final = tuple(bpy.data.objects[0].location)
        bpy.ops.ed.undo_push()

        all_ids_final_before_undoredo = self.get_all_ids()

        bpy.ops.ed.undo()
        self.assertEqual({id_data.pointer: id_data for id_data in all_ids_final_before_undoredo},
                         {id_data.pointer: id_data for id_data in self.get_all_ids()})
        self.assertEqual(tuple(bpy.data.objects[0].location), object_location_init)
        bpy.ops.ed.redo()
        self.assertEqual({id_data.pointer: id_data for id_data in all_ids_final_before_undoredo},
                         {id_data.pointer: id_data for id_data in self.get_all_ids()})
        self.assertEqual(tuple(bpy.data.objects[0].location), object_location_final)

        bpy.ops.ed.undo()
        bpy.ops.ed.undo()
        self.assertEqual({id_data.pointer: id_data for id_data in all_ids_init},
                         {id_data.pointer: id_data for id_data in self.get_all_ids()})

        # Undoing removed all IDs generated by the call to `gen_library_data_()`, so when redoing,
        # their invariant and session identifiers whould rmain unchanged,
        # but their pointer address should be different (as redo will re-allocate them).
        bpy.ops.ed.redo()
        all_ids_final_after_undoredo = self.get_all_ids()
        self.assertNotEqual({id_data.pointer: id_data for id_data in all_ids_final_before_undoredo},
                            {id_data.pointer: id_data for id_data in all_ids_final_after_undoredo})
        self.assertEqual({id_data.as_session_key(): id_data for id_data in all_ids_final_before_undoredo},
                         {id_data.as_session_key(): id_data for id_data in all_ids_final_after_undoredo})
        self.assertEqual({id_data.as_invariant_key(): id_data for id_data in all_ids_final_before_undoredo},
                         {id_data.as_invariant_key(): id_data for id_data in all_ids_final_after_undoredo})
        self.assertEqual(tuple(bpy.data.objects[0].location), object_location_init)

        # Second redo should re-use all exisitng IDs, even though some of their data is modified.
        bpy.ops.ed.redo()
        self.assertEqual({id_data.pointer: id_data for id_data in all_ids_final_after_undoredo},
                         {id_data.pointer: id_data for id_data in self.get_all_ids()})
        self.assertEqual(tuple(bpy.data.objects[0].location), object_location_final)

    # TODO: add undo testing of linking, making local, linking as packed data, making liboverrides, etc.


TESTS = (
    TestGlobalUndo,
)


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    description = (
        "Test basic global (aka memfile) undo and redo component.\n"
        "\n"
        "IMPORTANT: This test should be run in Blender with `--log \"*undo*\" --log-level debug`")
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
        help="Where to output temp saved blendfiles",
        required=True,
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
