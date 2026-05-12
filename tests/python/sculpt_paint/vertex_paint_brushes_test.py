# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */
"""
blender -b --factory-startup --python tests/python/sculpt_paint/vertex_paint_brushes_test.py -- --testdir tests/files/mesh_paint/
"""

__all__ = (
    "main",
)

import numpy as np
import math
import os
import pathlib
import unittest
import sys

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.test_helpers import set_view3d_context_override, generate_stroke

args = None


def get_attribute_data(
        attribute_name='Attribute',
        attribute_domain='CORNER',
        attribute_size=4,
        attribute_type=np.float32):
    mesh = bpy.context.object.data

    num_elements = mesh.attributes.domain_size(attribute_domain)
    attribute_data = np.zeros((num_elements * attribute_size), dtype=attribute_type)

    attribute = mesh.attributes.get(attribute_name)
    meta_attribute = 'color'

    if attribute:
        attribute.data.foreach_get(meta_attribute, np.ravel(attribute_data))

    return attribute_data


class MeshBrushTests(unittest.TestCase):
    """
    Test that none of the included brushes create NaN or inf valued vertices
    """

    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "30k_monkey.blend"), load_ui=False)
        bpy.ops.ed.undo_push()
        bpy.ops.paint.vertex_paint_toggle()

    def _activate_brush(self, brush):
        result = bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_vertex.blend/Brush/{}'.format(brush))
        self.assertEqual({'FINISHED'}, result)

    def _check_paint_stroke(self):
        initial_data = get_attribute_data()

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.paint.vertex_paint(stroke=generate_stroke(context_override), override_location=True)

        new_data = get_attribute_data()

        # Note, depending on if the tests are run with asserts enabled or not, the test may fail before this point
        # inside blender itself.
        all_valid = all([not math.isinf(channel) and not math.isnan(channel) for channel in new_data])
        any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
        self.assertTrue(all_valid, "All color components should be rational values")
        self.assertTrue(any_different, "At least one color component should be different from its original value")

    def test_airbrush_brush_creates_valid_data(self):
        self._activate_brush("Airbrush")
        self._check_paint_stroke()

    def test_paint_hard_brush_creates_valid_data(self):
        self._activate_brush("Paint Hard")
        self._check_paint_stroke()

    def test_paint_hard_pressure_brush_creates_valid_data(self):
        self._activate_brush("Paint Hard Pressure")
        self._check_paint_stroke()

    def test_paint_soft_brush_creates_valid_data(self):
        self._activate_brush("Paint Soft")
        self._check_paint_stroke()

    def test_paint_soft_pressure_brush_creates_valid_data(self):
        self._activate_brush("Paint Soft Pressure")
        self._check_paint_stroke()

    def test_average_brush_creates_valid_data(self):
        self._activate_brush("Paint Hard")
        self._check_paint_stroke()

        self._activate_brush("Average")
        self._check_paint_stroke()

    def test_blur_brush_creates_valid_data(self):
        self._activate_brush("Paint Hard")
        self._check_paint_stroke()

        self._activate_brush("Blur")
        self._check_paint_stroke()

    def test_smear_brush_creates_valid_data(self):
        self._activate_brush("Paint Hard")
        self._check_paint_stroke()

        self._activate_brush("Smear")
        self._check_paint_stroke()


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
