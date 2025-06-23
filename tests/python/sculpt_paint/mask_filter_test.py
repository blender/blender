# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */

__all__ = (
    "main",
)

import math
import unittest
import sys
import pathlib
import numpy as np

import bpy

"""
blender -b --factory-startup --python tests/python/sculpt_paint/mask_filter_test.py -- --testdir tests/files/sculpting/
"""

args = None


class GrowMaskTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "partially_masked_sphere.blend"), load_ui=False)
        bpy.ops.ed.undo_push()

    def test_grow_increases_number_of_masked_vertices(self):
        mesh = bpy.context.object.data
        mask_attr = mesh.attributes['.sculpt_mask']

        num_vertices = mesh.attributes.domain_size('POINT')

        old_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', old_mask_data)

        bpy.ops.sculpt.mask_filter(filter_type='GROW')

        new_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', new_mask_data)

        self.assertGreater(np.count_nonzero(new_mask_data), np.count_nonzero(old_mask_data))


class ShrinkMaskTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "partially_masked_sphere.blend"), load_ui=False)
        bpy.ops.ed.undo_push()

    def test_shrink_decreases_number_of_masked_vertices(self):
        mesh = bpy.context.object.data
        mask_attr = mesh.attributes['.sculpt_mask']

        num_vertices = mesh.attributes.domain_size('POINT')

        old_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', old_mask_data)

        bpy.ops.sculpt.mask_filter(filter_type='SHRINK')

        new_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', new_mask_data)

        self.assertLess(np.count_nonzero(new_mask_data), np.count_nonzero(old_mask_data))


def main():
    global args
    import argparse

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)

    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
