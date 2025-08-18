# SPDX-FileCopyrightText: 2020-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# To run all tests, use
# BLENDER_VERBOSE=1 ./bin/blender --background ../tests/files/sequence_editing/vse_load_meta_stack.blend --python ../blender/tests/python/sequencer_load_meta_stack.py
# (that assumes the test is run from a build directory in the same directory as the source code)
import bpy

import argparse
import sys
import unittest


class SequencerLoadMetastaskTest(unittest.TestCase):
    def get_sequence_editor(self):
        return bpy.context.scene.sequence_editor

    def test_meta_stack(self):
        sequence_editor = self.get_sequence_editor()

        meta_stack = sequence_editor.meta_stack
        self.assertEqual(len(meta_stack), 1)
        self.assertEqual(meta_stack[0].name, "MetaStrip")

        self.assertEqual(len(meta_stack[0].sequences), 1)
        self.assertEqual(meta_stack[0].sequences[0].name, "Color")

        # accesses ed->seqbasep through screen_ctx_selected_editable_sequences
        bpy.context.copy()


def main():
    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    parser = argparse.ArgumentParser()

    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
