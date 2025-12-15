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
blender -b --factory-startup --python tests/python/sculpt_paint/weight_paint_brushes_test.py -- --testdir tests/files/mesh_paint/
"""

args = None


def set_view3d_context_override(context_override):
    """
    Set context override to become the first viewport in the active workspace

    The ``context_override`` is expected to be a copy of an actual current context
    obtained by `context.copy()`
    """

    for area in context_override["screen"].areas:
        if area.type != 'VIEW_3D':
            continue
        for space in area.spaces:
            if space.type != 'VIEW_3D':
                continue
            for region in area.regions:
                if region.type != 'WINDOW':
                    continue
                context_override["area"] = area
                context_override["region"] = region


def generate_stroke(context, start_over_mesh=False):
    """
    Generate stroke for the bpy.ops.sculpt.brush_stroke operator

    The generated stroke coves the full plane diagonal.
    """
    import bpy
    from mathutils import Vector

    template = {
        "name": "stroke",
        "mouse": (0.0, 0.0),
        "mouse_event": (0, 0),
        "is_start": True,
        "location": (0, 0, 0),
        "pressure": 1.0,
        "time": 1.0,
        "size": 1.0,
        "x_tilt": 0,
        "y_tilt": 0
    }

    num_steps = 100
    if start_over_mesh:
        start = Vector((context['area'].width // 2, context['area'].height // 2))
    else:
        start = Vector((context['area'].width, context['area'].height))

    end = Vector((0, 0))
    delta = (end - start) / (num_steps - 1)

    stroke = []
    for i in range(num_steps):
        step = template.copy()
        step["mouse_event"] = start + delta * i
        stroke.append(step)

    return stroke


def get_weights(ob, vgroup):
    group_index = vgroup.index
    for i, v in enumerate(ob.data.vertices):
        for g in v.groups:
            if g.group == group_index:
                yield (i, g.weight)
                break


def get_attribute_data():
    obj = bpy.context.object
    mesh = bpy.context.object.data

    num_elements = mesh.attributes.domain_size('POINT')
    attribute_data = np.zeros(num_elements, dtype=np.float32)

    if obj.vertex_groups.get('Group'):
        vgroup = obj.vertex_groups[0]
        for (idx, weight) in list(get_weights(obj, vgroup)):
            attribute_data[idx] = weight

    return attribute_data


class MeshBrushTests(unittest.TestCase):
    """
    Test that none of the included brushes create NaN or inf valued vertices
    """

    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "30k_monkey.blend"), load_ui=False)
        bpy.ops.ed.undo_push()
        bpy.ops.paint.weight_paint_toggle()

    def _activate_brush(self, brush):
        result = bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_weight.blend/Brush/{}'.format(brush))
        self.assertEqual({'FINISHED'}, result)

    def _check_stroke(self):
        initial_data = get_attribute_data()

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.paint.weight_paint(stroke=generate_stroke(context_override), override_location=True)

        new_data = get_attribute_data()

        # Note, depending on if the tests are run with asserts enabled or not, the test may fail before this point
        # inside blender itself.
        all_valid = all([not math.isinf(weight) and not math.isnan(weight) for weight in new_data])
        any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
        self.assertTrue(all_valid, "All weights should be rational values")
        self.assertTrue(any_different, "At least one weight should be different from its original value")

    def test_paint_brush_creates_valid_data(self):
        self._activate_brush("Paint")
        self._check_stroke()

    def test_average_brush_creates_valid_data(self):
        self._activate_brush("Paint")
        self._check_stroke()

        self._activate_brush("Average")
        self._check_stroke()

    def test_blur_brush_creates_valid_data(self):
        self._activate_brush("Paint")
        self._check_stroke()

        self._activate_brush("Blur")
        self._check_stroke()

    def test_smear_brush_creates_valid_data(self):
        self._activate_brush("Paint")
        self._check_stroke()

        self._activate_brush("Smear")
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
