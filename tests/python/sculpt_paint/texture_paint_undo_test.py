# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */
"""
blender -b --factory-startup --python tests/python/sculpt_paint/texture_paint_undo_test.py
"""

__all__ = (
    "main",
)

import os
import sys
import unittest

import numpy as np

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.test_helpers import (
    set_view3d_context_override,
    set_image_editor_context_override,
    generate_monkey,
    BackendType,
)


def get_image_pixels(image_name):
    image = bpy.data.images[image_name]
    pixels = np.empty(len(image.pixels), dtype=np.float32)
    image.pixels.foreach_get(pixels)
    return pixels


def generate_horizontal_stroke(to_region, y_factor):
    num_steps = 50

    stroke = []
    for i in range(num_steps):
        x_factor = 0.2 + 0.6 * i / (num_steps - 1)
        stroke.append({
            "name": "stroke",
            "mouse": (0.0, 0.0),
            "mouse_event": to_region(x_factor, y_factor),
            "is_start": i == 0,
            "location": (0, 0, 0),
            "pressure": 1.0,
            "time": 1.0,
            "size": 1.0,
            "x_tilt": 0,
            "y_tilt": 0,
        })
    return stroke


class TexturePaintUndoTest(unittest.TestCase):
    """
    Test image undo steps mixed with memfile undo steps that change the image.
    """

    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.ed.undo_push()

    def _activate_paint_brush(self):
        with bpy.context.temp_override(**self.context_override):
            result = bpy.ops.brush.asset_activate(
                asset_library_type='ESSENTIALS',
                relative_asset_identifier='brushes/essentials_brushes-mesh_texture.blend/Brush/Paint Hard')
        self.assertEqual({'FINISHED'}, result)
        bpy.context.tool_settings.image_paint.unified_paint_settings.color = (0.0, 0.0, 0.0)

    def _view3d_startup(self):
        generate_monkey(BackendType.MESH)
        bpy.ops.paint.texture_paint_toggle()
        bpy.ops.paint.add_texture_paint_slot(
            type='BASE_COLOR',
            slot_type='IMAGE',
            name="Base_Color",
            color=(1.0, 1.0, 1.0, 1.0),
            width=512,
            height=512,
            generated_type='BLANK')
        bpy.ops.ed.undo_push(message="Add Texture Paint Slot")

        self.context_override = bpy.context.copy()
        set_view3d_context_override(self.context_override)
        region = self.context_override["region"]
        self.to_region = lambda x, y: (region.width * x, region.height * y)

        self._activate_paint_brush()
        return "Base_Color"

    def _image_editor_startup(self):
        # Use an image editor from the startup file, as the view of a new one is only
        # initialized when drawing, which does not happen in background mode.
        self.context_override = bpy.context.copy()
        self.context_override["screen"] = bpy.data.screens["Texture Paint"]
        set_image_editor_context_override(self.context_override)
        region = self.context_override["region"]
        space = self.context_override["area"].spaces.active

        image = bpy.data.images.new("Paint", 1024, 1024)
        image.generated_color = (1.0, 1.0, 1.0, 1.0)
        space.image = image
        space.mode = 'PAINT'
        bpy.ops.ed.undo_push(message="New Image")

        self.to_region = lambda x, y: region.view2d.view_to_region(x, y, clip=False)

        self._activate_paint_brush()
        return image.name

    def _paint_stroke(self, image_name, y_factor):
        pixels_before = get_image_pixels(image_name)
        with bpy.context.temp_override(**self.context_override):
            bpy.ops.paint.image_paint(stroke=generate_horizontal_stroke(self.to_region, y_factor))
        pixels = get_image_pixels(image_name)
        self.assertFalse(np.array_equal(pixels, pixels_before), "Stroke should change pixels")
        return pixels

    def _regenerate_image(self, image_name):
        bpy.data.images[image_name].generated_type = 'UV_GRID'
        bpy.ops.ed.undo_push(message="Generated Type")
        return get_image_pixels(image_name)

    def _assert_pixels_equal(self, image_name, pixels, msg):
        self.assertTrue(np.array_equal(get_image_pixels(image_name), pixels), msg)

    def test_view3d_undo_after_memfile_step(self):
        # An image step should not share tiles with an older image step when a
        # memfile step in between changed the image.
        image = self._view3d_startup()

        self._paint_stroke(image, 0.55)
        uv_grid = self._regenerate_image(image)
        after_b = self._paint_stroke(image, 0.45)

        bpy.ops.ed.undo()                   # Undo: second stroke.
        self._assert_pixels_equal(image, uv_grid, "Undo should restore the regenerated image")
        bpy.ops.ed.undo()                   # Undo: generated type.
        self.assertEqual(bpy.data.images[image].generated_type, 'BLANK')
        self.assertEqual(bpy.context.object.mode, 'TEXTURE_PAINT')

        bpy.ops.ed.redo()                   # Redo: generated type.
        self.assertEqual(bpy.data.images[image].generated_type, 'UV_GRID')
        bpy.ops.ed.redo()                   # Redo: second stroke.
        self._assert_pixels_equal(image, after_b, "Redo should restore second stroke")

    def test_view3d_undo_redo_after_pack_and_generate(self):
        # Pack and change image source in between painting.
        image = self._view3d_startup()
        initial = get_image_pixels(image)

        self._paint_stroke(image, 0.55)

        bpy.data.images[image].pack()
        bpy.ops.ed.undo_push(message="Pack")
        bpy.data.images[image].source = 'GENERATED'
        uv_grid = self._regenerate_image(image)

        after_b = self._paint_stroke(image, 0.45)
        after_c = self._paint_stroke(image, 0.65)

        for _ in range(3):
            bpy.ops.ed.undo()               # Undo: third stroke.
            self._assert_pixels_equal(image, after_b, "Undo should restore second stroke")
            bpy.ops.ed.undo()               # Undo: second stroke.
            self._assert_pixels_equal(image, uv_grid, "Undo should restore the generated image")
            bpy.ops.ed.redo()               # Redo: second stroke.
            self._assert_pixels_equal(image, after_b, "Redo should restore second stroke")
            bpy.ops.ed.redo()               # Redo: third stroke.
            self._assert_pixels_equal(image, after_c, "Redo should restore third stroke")

        # Undo and redo everything, across the memfile steps.
        for _ in range(5):
            bpy.ops.ed.undo()
        self._assert_pixels_equal(image, initial, "Undo should restore initial image")
        for _ in range(5):
            bpy.ops.ed.redo()
        self._assert_pixels_equal(image, after_c, "Redo should restore third stroke")

    def test_image_editor_undo_after_memfile_step(self):
        # Image editor variant of `test_view3d_undo_after_memfile_step`.
        image = self._image_editor_startup()

        self._paint_stroke(image, 0.75)
        self._paint_stroke(image, 0.55)
        uv_grid = self._regenerate_image(image)
        after_b = self._paint_stroke(image, 0.45)

        bpy.ops.ed.undo()                   # Undo: second stroke.
        self._assert_pixels_equal(image, uv_grid, "Undo should restore the regenerated image")
        bpy.ops.ed.redo()                   # Redo: second stroke.
        self._assert_pixels_equal(image, after_b, "Redo should restore second stroke")

    def test_image_editor_undo_redo_each_step_after_pack(self):
        # Undoing pack regenerates the image buffer, test that we properly restore
        # partial image undo steps from painting before packing.
        image = self._image_editor_startup()
        states = [("New Image", get_image_pixels(image))]

        for y_factor in (0.3, 0.5, 0.7):
            states.append(("Stroke", self._paint_stroke(image, y_factor)))

        bpy.data.images[image].pack()
        bpy.ops.ed.undo_push(message="Pack")
        states.append(("Pack", get_image_pixels(image)))
        bpy.data.images[image].source = 'GENERATED'
        bpy.ops.ed.undo_push(message="Source")
        states.append(("Source", get_image_pixels(image)))
        states.append(("Generated Type", self._regenerate_image(image)))

        for y_factor in (0.35, 0.55, 0.75):
            states.append(("Stroke", self._paint_stroke(image, y_factor)))

        for i in reversed(range(len(states) - 1)):
            bpy.ops.ed.undo()
            self._assert_pixels_equal(image, states[i][1], f"Undo to {states[i][0]} (step {i})")
        for i in range(1, len(states)):
            bpy.ops.ed.redo()
            self._assert_pixels_equal(image, states[i][1], f"Redo to {states[i][0]} (step {i})")


def main():
    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    unittest.main(argv=argv, verbosity=2)


if __name__ == "__main__":
    main()
