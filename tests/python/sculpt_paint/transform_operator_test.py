# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "main",
)

import unittest
import sys
import pathlib

import bpy

"""
blender -b --factory-startup --python tests/python/sculpt_paint/transform_operator_test.py -- --testdir tests/files/sculpting/
"""

args = None

# TODO: Current test should be rewritten with Mesh comparison
# framework(see voxel_remesh_compare_test.py) to reduce the amount of
# code, once framework supports context override.


class TransformTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "non_uniform_scaling_monkey.blend"), load_ui=False)

    def test_scaling_and_rotating_in_sculpt_mode(self):
        test_monkey = bpy.data.objects['test_Monkey']
        expected_monkey = bpy.data.objects['Expected_test_Monkey']

        bpy.context.view_layer.objects.active = test_monkey

        window = bpy.context.window_manager.windows[0]
        viewarea_3D = next(area for area in window.screen.areas if area.type == 'VIEW_3D')
        window_region = next(region for region in viewarea_3D.regions if region.type == 'WINDOW')
        override_context = {
            "window": window,
            "area": viewarea_3D,
            "region": window_region,
            "space_data": viewarea_3D.spaces.active,
            "region_data": viewarea_3D.spaces.active.region_3d,
            "active_object": test_monkey,
            "object": test_monkey
        }

        bpy.ops.ed.undo_push('EXEC_DEFAULT')
        with bpy.context.temp_override(**override_context):
            bpy.ops.object.mode_set(mode='SCULPT')
            bpy.ops.transform.rotate(value=1.5708, orient_axis='Y', orient_type='GLOBAL')
            bpy.ops.object.mode_set(mode='OBJECT')

        result = expected_monkey.data.unit_test_compare(mesh=test_monkey.data)
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
