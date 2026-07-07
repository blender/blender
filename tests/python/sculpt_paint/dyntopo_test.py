# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */

__all__ = (
    "main",
)

import unittest
import sys
import pathlib
import numpy as np

import bpy

"""
blender -b --factory-startup --python tests/python/dyntopo_test.py -- --testdir tests/files/sculpting/
"""

args = None


class DetailFloodFillTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.ed.undo_push()

        bpy.ops.mesh.primitive_cube_add()
        bpy.ops.sculpt.sculptmode_toggle()
        bpy.ops.sculpt.dynamic_topology_toggle()

    def test_operator_subdivides_mesh(self):
        """Test that the operator generates a mesh with appropriately sized edges."""

        max_edge_length = 1.0
        # Based on the detail_size::EDGE_LENGTH_MIN_FACTOR constant
        min_edge_length = max_edge_length * 0.4

        bpy.context.scene.tool_settings.sculpt.detail_type_method = 'CONSTANT'
        bpy.context.scene.tool_settings.sculpt.constant_detail_resolution = max_edge_length

        ret_val = bpy.ops.sculpt.detail_flood_fill()
        self.assertEqual({'FINISHED'}, ret_val)

        # Toggle to ensure the mesh data is refreshed.
        bpy.ops.sculpt.dynamic_topology_toggle()

        mesh = bpy.context.object.data
        for edge in mesh.edges:
            v0 = mesh.vertices[edge.vertices[0]]
            v1 = mesh.vertices[edge.vertices[1]]

            length = (v0.co - v1.co).length

            self.assertGreaterEqual(
                length,
                min_edge_length,
                f"Edge between {v0.index} and {v1.index} should be longer than minimum length")
            self.assertLessEqual(
                length,
                max_edge_length,
                f"Edge between {v0.index} and {v1.index} should be shorter than maximum length")


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
