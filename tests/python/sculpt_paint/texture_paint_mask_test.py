# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */
"""
blender -b --factory-startup --python tests/python/sculpt_paint/texture_paint_mask_test.py -- --testdir tests/files/mesh_paint/
"""

__all__ = (
    "main",
)

import enum
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


def get_image_data(image_name="Base_Color"):
    return list(bpy.data.images[image_name].pixels)


class ExperimentalMaskTests(unittest.TestCase):
    """
    Test that the stencil mask restricts texture painting as expected
    """

    def _initialize(self, data_type: DataType, mask_color):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.context.preferences.experimental.use_3d_texture_paint = True
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

        bpy.ops.image.new(
            name="Mask",
            width=256,
            height=256,
            color=mask_color,
            float=data_type == DataType.FLOAT)
        bpy.context.scene.tool_settings.image_paint.stencil_image = bpy.data.images["Mask"]
        bpy.context.scene.tool_settings.image_paint.use_stencil_layer = True

    def _activate_brush(self, brush):
        result = bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_texture.blend/Brush/{}'.format(brush))
        self.assertEqual({'FINISHED'}, result)

    def _do_stroke(self):
        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.paint.image_paint(stroke=generate_stroke(context_override), override_location=True)

    def _is_pixel_fully_painted(self, image_data, index):
        brush_color = bpy.context.tool_settings.image_paint.unified_paint_settings.color
        brush_color = (brush_color.r, brush_color.g, brush_color.b)
        expected_color = (image_data[index], image_data[index + 1], image_data[index + 2]) == brush_color
        expected_alpha = image_data[index + 3] == 1.0
        return expected_color and expected_alpha

    @unittest.skipIf(bpy.app.version_cycle != 'alpha', "Experimental features are only testable in alpha")
    def test_mask_no_restriction(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type, mask_color=(0.0, 0.0, 0.0, 1.0))
                self._activate_brush("Paint Hard")

                initial_data = get_image_data()
                self._do_stroke()
                new_data = get_image_data()

                any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
                any_fully_painted = any([self._is_pixel_fully_painted(image_data=new_data, index=channel)
                                         for channel in range(0, len(new_data), 4)])
                self.assertTrue(any_different,
                                "At least one color component should be different from its original value")
                self.assertTrue(any_fully_painted, "Mask should allow at least one pixel to be fully painted")

    @unittest.skipIf(bpy.app.version_cycle != 'alpha', "Experimental features are only testable in alpha")
    def test_mask_partial_restriction(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type, mask_color=(0.5, 0.5, 0.5, 1.0))
                self._activate_brush("Paint Hard")

                initial_data = get_image_data()
                self._do_stroke()
                new_data = get_image_data()

                any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
                none_fully_painted = all([not self._is_pixel_fully_painted(image_data=new_data, index=channel)
                                          for channel in range(0, len(new_data), 4)])
                self.assertTrue(any_different,
                                "At least one color component should be different from its original value")
                self.assertTrue(none_fully_painted, "Mask should prevent all pixels from being fully painted")

    @unittest.skipIf(bpy.app.version_cycle != 'alpha', "Experimental features are only testable in alpha")
    def test_mask_full_restriction(self):
        for data_type in DataType:
            with self.subTest(data_type):
                self._initialize(data_type, mask_color=(1.0, 1.0, 1.0, 1.0))
                self._activate_brush("Paint Hard")

                initial_data = get_image_data()
                self._do_stroke()
                new_data = get_image_data()

                no_difference = all([orig == new for (orig, new) in zip(initial_data, new_data)])
                self.assertTrue(no_difference, "Mask should prevent any changes to color components")


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
