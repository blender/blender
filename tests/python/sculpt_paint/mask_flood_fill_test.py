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
blender -b --factory-startup --python tests/python/sculpt_paint/mask_flood_fill_test.py -- --testdir tests/files/sculpting/
"""

args = None


class ClearMaskTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "partially_masked_sphere.blend"), load_ui=False)
        bpy.ops.ed.undo_push()

    def test_value_removes_attribute(self):
        mesh = bpy.context.object.data

        bpy.ops.paint.mask_flood_fill(mode='VALUE', value=0)

        self.assertFalse(mesh.attributes.get('.sculpt_mask'))


class InvertMaskTest(unittest.TestCase):
    def test_invert_applies_correct_values(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "partially_masked_sphere.blend"), load_ui=False)
        bpy.ops.ed.undo_push()

        mesh = bpy.context.object.data
        mask_attr = mesh.attributes['.sculpt_mask']

        num_vertices = mesh.attributes.domain_size('POINT')

        old_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', old_mask_data)

        bpy.ops.paint.mask_flood_fill(mode='INVERT')

        new_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', new_mask_data)

        expected_mask_data = np.array([1.0 - m for m in old_mask_data])

        self.assertEqual(expected_mask_data.tolist(), new_mask_data.tolist())

    def test_invert_on_empty_fills_mesh(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.ed.undo_push()

        bpy.ops.mesh.primitive_monkey_add()
        bpy.ops.sculpt.sculptmode_toggle()

        bpy.ops.paint.mask_flood_fill(mode='INVERT')

        mesh = bpy.context.object.data
        mask_attr = mesh.attributes['.sculpt_mask']

        num_vertices = mesh.attributes.domain_size('POINT')

        new_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', new_mask_data)

        expected_mask_data = np.ones(num_vertices, dtype=np.float32)

        self.assertEqual(expected_mask_data.tolist(), new_mask_data.tolist())


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
