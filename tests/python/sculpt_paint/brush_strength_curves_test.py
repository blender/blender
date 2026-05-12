# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */
"""
blender -b --factory-startup --python tests/python/sculpt_paint/brush_strength_curves_test.py -- --testdir tests/files/sculpting/
"""

__all__ = (
    "main",
)

import math
import os
import pathlib
import numpy as np
import sys
import unittest

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.test_helpers import set_view3d_context_override, generate_stroke

args = None


class BrushCurvesTest(unittest.TestCase):
    """
    Test that using a basic "Draw" brush stroke with each of the given brush curve presets doesn't produce invalid
    deformations in the mesh, usually resulting in what looks like geometry disappearing to the user.
    """

    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.ed.undo_push()

        bpy.ops.mesh.primitive_monkey_add()
        bpy.ops.sculpt.sculptmode_toggle()

    def _check_stroke(self):
        # Ideally, we would use something like pytest and parameterized tests here, but this helper function is an
        # alright solution for now...
        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.sculpt.brush_stroke(stroke=generate_stroke(context_override), override_location=True)

        mesh = bpy.context.object.data
        position_attr = mesh.attributes['position']

        num_vertices = mesh.attributes.domain_size('POINT')

        position_data = np.zeros((num_vertices * 3), dtype=np.float32)
        position_attr.data.foreach_get('vector', np.ravel(position_data))

        # Note, depending on if the tests are run with asserts enabled or not, the test may fail before this point
        # inside blender itself.
        all_valid = all([not math.isinf(pos) and not math.isnan(pos) for pos in position_data])
        self.assertTrue(all_valid, "All position components should be rational values")

    def test_smooth_preset_curve_creates_valid_stroke(self):
        bpy.context.tool_settings.sculpt.brush.curve_distance_falloff_preset = 'SMOOTH'
        self._check_stroke()

    def test_smoother_preset_curve_creates_valid_stroke(self):
        bpy.context.tool_settings.sculpt.brush.curve_distance_falloff_preset = 'SMOOTHER'
        self._check_stroke()

    def test_sphere_preset_curve_creates_valid_stroke(self):
        bpy.context.tool_settings.sculpt.brush.curve_distance_falloff_preset = 'SPHERE'
        self._check_stroke()

    def test_root_preset_curve_creates_valid_stroke(self):
        bpy.context.tool_settings.sculpt.brush.curve_distance_falloff_preset = 'ROOT'
        self._check_stroke()

    def test_sharp_preset_curve_creates_valid_stroke(self):
        bpy.context.tool_settings.sculpt.brush.curve_distance_falloff_preset = 'SHARP'
        self._check_stroke()

    def test_linear_preset_curve_creates_valid_stroke(self):
        bpy.context.tool_settings.sculpt.brush.curve_distance_falloff_preset = 'LIN'
        self._check_stroke()

    def test_sharper_preset_curve_creates_valid_stroke(self):
        bpy.context.tool_settings.sculpt.brush.curve_distance_falloff_preset = 'POW4'
        self._check_stroke()

    def test_inverse_square_preset_curve_creates_valid_stroke(self):
        bpy.context.tool_settings.sculpt.brush.curve_distance_falloff_preset = 'INVSQUARE'
        self._check_stroke()

    def test_constant_preset_curve_creates_valid_stroke(self):
        bpy.context.tool_settings.sculpt.brush.curve_distance_falloff_preset = 'CONSTANT'
        self._check_stroke()


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
