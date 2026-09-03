# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */
"""
blender -b --factory-startup --python tests/python/sculpt_paint/texture_paint_brushes_test.py -- --testdir tests/files/mesh_paint/
"""

__all__ = (
    "main",
)

import enum
import math
import os
import pathlib
import unittest
import sys

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.test_helpers import set_view3d_context_override, generate_stroke, generate_monkey, BackendType

args = None


@enum.unique
class DataType(enum.Enum):
    BYTE = 0
    FLOAT = 1


def get_image_data(image_name):
    return list(bpy.data.images[image_name].pixels)


class ExperimentalBrushTests(unittest.TestCase):
    """
    Test that none of the included brushes create NaN or inf valued pixels
    """

    def _initialize(self, data_type: DataType):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.context.preferences.experimental.use_sculpt_texture_paint = True
        bpy.ops.ed.undo_push()
        generate_monkey(BackendType.MESH)
        bpy.ops.sculpt.sculptmode_toggle()

        bpy.ops.paint.add_texture_paint_slot(
            type='BASE_COLOR',
            slot_type='IMAGE',
            name="Base_Color",
            color=(
                1.0,
                1.0,
                1.0,
                1.0),
            width=512,
            height=512,
            alpha=True,
            generated_type='BLANK',
            float=data_type == DataType.FLOAT)

    def _activate_brush(self, brush):
        result = bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_sculpt.blend/Brush/{}'.format(brush))
        self.assertEqual({'FINISHED'}, result)

    def _check_paint_stroke(self, image_name="Base_Color"):
        initial_data = get_image_data(image_name=image_name)

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.sculpt.brush_stroke(stroke=generate_stroke(context_override), override_location=True)

        new_data = get_image_data(image_name=image_name)

        # Note, depending on if the tests are run with asserts enabled or not, the test may fail before this point
        # inside blender itself.
        all_valid = all([not math.isinf(channel) and not math.isnan(channel) for channel in new_data])
        any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
        self.assertTrue(all_valid, "All color components should be rational values")
        self.assertTrue(any_different, "At least one color component should be different from its original value")

    @unittest.skipIf(bpy.app.version_cycle != 'alpha', "Experimental features are only testable in alpha")
    def test_paint_hard_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Paint Hard")
                self._check_paint_stroke()


class BrushTests(unittest.TestCase):
    """
    Test that none of the included brushes create NaN or inf valued pixels
    """

    def _initialize(self, data_type: DataType):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.context.preferences.experimental.use_sculpt_texture_paint = False
        bpy.ops.ed.undo_push()
        generate_monkey(BackendType.MESH)
        bpy.ops.paint.texture_paint_toggle()

        bpy.ops.paint.add_texture_paint_slot(
            type='BASE_COLOR',
            slot_type='IMAGE',
            name="Base_Color",
            color=(
                1.0,
                1.0,
                1.0,
                1.0),
            width=512,
            height=512,
            alpha=True,
            generated_type='BLANK',
            float=data_type == DataType.FLOAT)

    def _activate_brush(self, brush):
        result = bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_texture.blend/Brush/{}'.format(brush))
        self.assertEqual({'FINISHED'}, result)

    def _check_paint_stroke(self, image_name="Base_Color"):
        initial_data = get_image_data(image_name=image_name)

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.paint.image_paint(stroke=generate_stroke(context_override))

        new_data = get_image_data(image_name=image_name)

        # Note, depending on if the tests are run with asserts enabled or not, the test may fail before this point
        # inside blender itself.
        all_valid = all([not math.isinf(channel) and not math.isnan(channel) for channel in new_data])
        any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
        self.assertTrue(all_valid, "All color components should be rational values")
        self.assertTrue(any_different, "At least one color component should be different from its original value")

    def test_airbrush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Airbrush")
                self._check_paint_stroke()

    def test_blur_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Paint Hard")
                self._check_paint_stroke()

                self._activate_brush("Blur")
                self._check_paint_stroke()

    def test_paint_hard_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Paint Hard")
                self._check_paint_stroke()

    def test_paint_hard_pressure_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Paint Hard Pressure")
                self._check_paint_stroke()

    def test_paint_soft_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Paint Soft")
                self._check_paint_stroke()

    def test_paint_soft_pressure_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Paint Soft Pressure")
                self._check_paint_stroke()

    def test_smear_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Paint Hard")
                self._check_paint_stroke()

                self._activate_brush("Smear")
                self._check_paint_stroke()

    def test_erase_hard_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Paint Hard")
                self._check_paint_stroke()

                self._activate_brush("Erase Hard")
                self._check_paint_stroke()

    def test_erase_hard_pressure_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Paint Hard")
                self._check_paint_stroke()

                self._activate_brush("Erase Hard Pressure")
                self._check_paint_stroke()

    def test_erase_soft_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Paint Hard")
                self._check_paint_stroke()

                self._activate_brush("Erase Soft")
                self._check_paint_stroke()

    def test_fill_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Fill")
                self._check_paint_stroke()

    def test_mask_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                mask_image = bpy.data.images.new("Mask", 512, 512, float_buffer=data_type == DataType.FLOAT)
                bpy.context.scene.tool_settings.image_paint.stencil_image = mask_image
                self._activate_brush("Mask")
                self._check_paint_stroke(image_name="Mask")

    def test_clone_brush_creates_valid_data(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type)
                self._activate_brush("Paint Hard")
                self._check_paint_stroke()

                self._activate_brush("Clone")
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

    unittest.main(argv=remaining, verbosity=2)


if __name__ == "__main__":
    main()
