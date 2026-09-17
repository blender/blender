# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */
"""
blender -b --factory-startup --python tests/python/sculpt_paint/grease_pencil_paint_tests.py -- --testdir tests/files/sculpt_paint/
"""

__all__ = (
    "main",
)

import math
import numpy as np
import os
import pathlib
import sys
import unittest

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.test_helpers import set_view3d_context_override, generate_stroke

args = None


def delete_attribute_from_all_drawings(grease_pencil, attribute_name):
    for layer in grease_pencil.layers:
        for frame in layer.frames:
            if frame.drawing.attributes.find(attribute_name) >= 0:
                attr = frame.drawing.attributes[attribute_name]
                frame.drawing.attributes.remove(attr)


class GreasePencilPaintTests(unittest.TestCase):
    """
    Test that painting some predefined test strokes result in the expected drawing
    """

    def setUp(self):
        bpy.ops.wm.open_mainfile(
            filepath=str(
                args.testdir /
                "../grease_pencil/grease_pencil_paint_tests.blend"),
            load_ui=False)
        bpy.ops.ed.undo_push()

    def prepare(self):
        data = bpy.data.grease_pencils.new("test")
        obj = bpy.data.objects.new("test", data)
        bpy.context.collection.objects.link(obj)
        layer = data.layers.new("layer")
        frame = layer.frames.new(0)
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.mode_set(mode='PAINT_GREASE_PENCIL')

    def cleanup(self):
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.delete()

    def test_stroke_generates_correct_drawing(self):
        self.prepare()
        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.grease_pencil.brush_stroke(stroke=generate_stroke(context_override))

        # Remove the 'init_time' and 'delta_time' attributes before comparing, because they will
        # differ by definition.
        delete_attribute_from_all_drawings(bpy.data.grease_pencils['expected'], "init_time")
        delete_attribute_from_all_drawings(bpy.data.grease_pencils['test'], "init_time")
        delete_attribute_from_all_drawings(bpy.data.grease_pencils['expected'], "delta_time")
        delete_attribute_from_all_drawings(bpy.data.grease_pencils['test'], "delta_time")

        # Remove the ".selection" attribute, because the domain it exists on depends on the last mode
        # the Grease Pencil object was in.
        delete_attribute_from_all_drawings(bpy.data.grease_pencils['expected'], ".selection")
        delete_attribute_from_all_drawings(bpy.data.grease_pencils['test'], ".selection")
        comparison = bpy.data.grease_pencils['expected'].unit_test_compare(
            grease_pencil=bpy.data.grease_pencils['test'], threshold=0.001)

        self.assertTrue(comparison == 'Same', comparison)
        self.cleanup()


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
