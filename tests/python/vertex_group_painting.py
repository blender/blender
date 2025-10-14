# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import sys
import pathlib

import bpy

"""
blender -b --factory-startup --python tests/python/vertex_group_paint.py -- --testdir tests/files/animation/
"""


class NormalizeAllTest(unittest.TestCase):
    """Test for normalizing all vertex groups on a mesh."""

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        self._load_test_file()

        # Make sure the initial state is what we expect/need for the tests.
        self.cube = bpy.data.objects['Cube']
        self.assertEqual(bpy.context.active_object, self.cube)
        self.assertEqual(len(self.cube.vertex_groups), 2)

    def _load_test_file(self):
        blendpath = str(args.testdir / "vertex_groups.blend")
        bpy.ops.wm.read_homefile(use_factory_startup=True)  # Just to be sure.
        bpy.ops.wm.open_mainfile(filepath=blendpath, load_ui=False)

    def test_normalize_all(self):
        # Initial state.
        self.cube.data.vertices[0].groups[0].weight = 0.375
        self.cube.data.vertices[0].groups[1].weight = 0.125
        self.cube.vertex_groups[0].lock_weight = False
        self.cube.vertex_groups[1].lock_weight = False

        # Attempt to normalize all vertex groups.  Should succeed.
        bpy.ops.object.vertex_group_normalize_all(group_select_mode='ALL', lock_active=False)
        self.assertEqual(self.cube.data.vertices[0].groups[0].weight, 0.75)
        self.assertEqual(self.cube.data.vertices[0].groups[1].weight, 0.25)

    def test_normalize_all_locked(self):
        # Initial state.
        self.cube.data.vertices[0].groups[0].weight = 0.375
        self.cube.data.vertices[0].groups[1].weight = 0.125
        self.cube.vertex_groups[0].lock_weight = True
        self.cube.vertex_groups[1].lock_weight = True

        # Attempt to normalize all vertex groups.  Should fail, leaving the weights as-is.
        with self.assertRaises(RuntimeError):
            bpy.ops.object.vertex_group_normalize_all(group_select_mode='ALL', lock_active=False)
        self.assertEqual(self.cube.data.vertices[0].groups[0].weight, 0.375)
        self.assertEqual(self.cube.data.vertices[0].groups[1].weight, 0.125)


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
