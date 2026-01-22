# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */

__all__ = (
    "main",
)

import unittest
import sys
import pathlib

import bpy
from mathutils import Vector

"""
blender -b --factory-startup --python tests/python/bl_object_modifier_multires.py -- --testdir tests/files/sculpting/
"""

args = None


class ApplyBase(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "apply_base_monkey.blend"), load_ui=False)

    def test_subdividing_cube_results_in_same_mesh(self):
        bpy.ops.mesh.primitive_monkey_add()
        new_cube = bpy.context.object

        multires_mod = new_cube.modifiers.new("Multires", 'MULTIRES')
        bpy.ops.object.multires_subdivide(modifier="Multires")
        bpy.ops.object.multires_base_apply(modifier="Multires")
        bpy.ops.object.modifier_remove(modifier="Multires")

        expected_mesh = bpy.data.objects['Expected_Base_Mesh']
        result = expected_mesh.data.unit_test_compare(mesh=new_cube.data)
        self.assertEqual(result, 'Same')


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


if __name__ == '__main__':
    main()
