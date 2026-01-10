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
blender -b --factory-startup --python tests/python/sculpt_paint/sculpt_brushes_test.py -- --testdir tests/files/mesh_paint/
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


def get_attribute_data(
        attribute_name='position',
        attribute_domain='POINT',
        attribute_size=3,
        attribute_type=np.float32,
        is_color=False):
    mesh = bpy.context.object.data

    num_elements = mesh.attributes.domain_size(attribute_domain)
    attribute_data = np.zeros((num_elements * attribute_size), dtype=attribute_type)

    attribute = mesh.attributes.get(attribute_name)
    if is_color:
        meta_attribute = 'color'
    else:
        if attribute_size > 1:
            meta_attribute = 'vector'
        else:
            meta_attribute = 'value'

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
        bpy.ops.sculpt.sculptmode_toggle()

    def _activate_brush(self, brush):
        result = bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_sculpt.blend/Brush/{}'.format(brush))
        self.assertEqual({'FINISHED'}, result)

    def _check_stroke(self, start_over_mesh=False):
        # Ideally, we would use something like pytest and parameterized tests here, but this helper function is an
        # alright solution for now...

        initial_data = get_attribute_data()

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.sculpt.brush_stroke(
                stroke=generate_stroke(
                    context_override,
                    start_over_mesh),
                override_location=True)

        new_data = get_attribute_data()

        # Note, depending on if the tests are run with asserts enabled or not, the test may fail before this point
        # inside blender itself.
        all_valid = all([not math.isinf(pos) and not math.isnan(pos) for pos in new_data])
        any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
        self.assertTrue(all_valid, "All position components should be rational values")
        self.assertTrue(any_different, "At least one position should be different from its original value")

    def _check_mask_stroke(self):
        initial_data = get_attribute_data(
            attribute_name='.sculpt_mask',
            attribute_domain='POINT',
            attribute_size=1,
            attribute_type=np.float32)

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.sculpt.brush_stroke(stroke=generate_stroke(context_override), override_location=True)

        new_data = get_attribute_data(
            attribute_name='.sculpt_mask',
            attribute_domain='POINT',
            attribute_size=1,
            attribute_type=np.float32)

        # Note, depending on if the tests are run with asserts enabled or not, the test may fail before this point
        # inside blender itself.
        all_valid = all([not math.isinf(mask) and not math.isnan(mask) for mask in new_data])
        any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
        self.assertTrue(all_valid, "All mask values should be rational values")
        self.assertTrue(any_different, "At least one mask should be different from its original value")

    def _check_face_set_stroke(self):
        initial_data = get_attribute_data(
            attribute_name='.sculpt_face_set',
            attribute_domain='FACE',
            attribute_size=1,
            attribute_type=np.int32)

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.sculpt.brush_stroke(stroke=generate_stroke(context_override), override_location=True)

        new_data = get_attribute_data(
            attribute_name='.sculpt_face_set',
            attribute_domain='FACE',
            attribute_size=1,
            attribute_type=np.int32)

        # Note, depending on if the tests are run with asserts enabled or not, the test may fail before this point
        # inside blender itself.
        all_valid = all([face_set_id == 1 or face_set_id == 2 for face_set_id in new_data])
        any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
        self.assertTrue(all_valid, "All face set values should be 1 or 2 valued")
        self.assertTrue(any_different, "At least one face set should be different from its original value")

    def _check_paint_stroke(self):
        initial_data = get_attribute_data(
            attribute_name='Color',
            attribute_domain='POINT',
            attribute_size=4,
            attribute_type=np.float32,
            is_color=True)

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.sculpt.brush_stroke(stroke=generate_stroke(context_override), override_location=True)

        new_data = get_attribute_data(
            attribute_name='Color',
            attribute_domain='POINT',
            attribute_size=4,
            attribute_type=np.float32,
            is_color=True)

        # Note, depending on if the tests are run with asserts enabled or not, the test may fail before this point
        # inside blender itself.
        all_valid = all([not math.isinf(channel) and not math.isnan(channel) for channel in new_data])
        any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
        self.assertTrue(all_valid, "All color components should be rational values")
        self.assertTrue(any_different, "At least one color component should be different from its original value")

    def test_blob_brush_creates_valid_data(self):
        self._activate_brush("Blob")
        self._check_stroke()

    def test_clay_brush_creates_valid_data(self):
        self._activate_brush("Clay")
        self._check_stroke()

    def test_clay_strips_brush_creates_valid_data(self):
        self._activate_brush("Clay Strips")
        self._check_stroke()

    def test_clay_thumb_brush_creates_valid_data(self):
        self._activate_brush("Clay Thumb")
        self._check_stroke()

    def test_crease_polish_brush_creates_valid_data(self):
        self._activate_brush("Crease Polish")
        self._check_stroke()

    def test_crease_sharp_brush_creates_valid_data(self):
        self._activate_brush("Crease Sharp")
        self._check_stroke()

    def test_draw_brush_creates_valid_data(self):
        self._activate_brush("Draw")
        self._check_stroke()

    def test_draw_sharp_brush_creates_valid_data(self):
        self._activate_brush("Draw Sharp")
        self._check_stroke()

    def test_inflate_deflate_brush_creates_valid_data(self):
        self._activate_brush("Inflate/Deflate")
        self._check_stroke()

    def test_fill_deepen_brush_creates_valid_data(self):
        self._activate_brush("Fill/Deepen")
        self._check_stroke()

    def test_flatten_contrast_brush_creates_valid_data(self):
        self._activate_brush("Flatten/Contrast")
        self._check_stroke()

    def test_plateau_brush_creates_valid_data(self):
        self._activate_brush("Plateau")
        self._check_stroke()

    def test_scrape_multiplane_brush_creates_valid_data(self):
        self._activate_brush("Scrape Multiplane")
        self._check_stroke()

    def test_scrape_fill_brush_creates_valid_data(self):
        self._activate_brush("Scrape/Fill")
        self._check_stroke()

    def test_smooth_brush_creates_valid_data(self):
        self._activate_brush("Smooth")
        self._check_stroke()

    def test_trim_brush_creates_valid_data(self):
        self._activate_brush("Trim")
        self._check_stroke()

    def test_twist_brush_creates_valid_data(self):
        self._activate_brush("Boundary")
        self._check_stroke()

    def test_elastic_grab_brush_creates_valid_data(self):
        self._activate_brush("Elastic Grab")
        self._check_stroke()

    def test_elastic_snake_hook_brush_creates_valid_data(self):
        self._activate_brush("Elastic Snake Hook")
        self._check_stroke()

    def test_grab_brush_creates_valid_data(self):
        self._activate_brush("Grab")
        self._check_stroke()

    def test_grab_2d_brush_creates_valid_data(self):
        self._activate_brush("Grab 2D")
        self._check_stroke()

    @unittest.skip("Requires raycast")
    def test_grab_silhouette_brush_creates_valid_data(self):
        self._activate_brush("Grab Silhouette")
        self._check_stroke(start_over_mesh=True)

    def test_nudge_brush_creates_valid_data(self):
        self._activate_brush("Nudge")
        self._check_stroke()

    def test_pinch_magnify_brush_creates_valid_data(self):
        self._activate_brush("Pinch/Magnify")
        self._check_stroke()

    @unittest.skip("Brush requires raycast")
    def test_pose_brush_creates_valid_data(self):
        self._activate_brush("Pose")
        self._check_stroke(start_over_mesh=True)

    def test_pull_brush_creates_valid_data(self):
        self._activate_brush("Pull")
        self._check_stroke()

    def test_relax_pinch_brush_creates_valid_data(self):
        self._activate_brush("Relax Pinch")
        self._check_stroke()

    def test_relax_slide_brush_creates_valid_data(self):
        self._activate_brush("Relax Slide")
        self._check_stroke()

    def test_snake_hook_brush_creates_valid_data(self):
        self._activate_brush("Snake Hook")
        self._check_stroke()

    def test_thumb_brush_creates_valid_data(self):
        self._activate_brush("Thumb")
        self._check_stroke()

    def test_twist_brush_creates_valid_data(self):
        self._activate_brush("Twist")
        self._check_stroke(start_over_mesh=True)

    def test_mask_brush_creates_valid_data(self):
        self._activate_brush("Mask")
        self._check_mask_stroke()

    def test_face_set_brush_creates_valid_data(self):
        self._activate_brush("Face Set Paint")
        self._check_face_set_stroke()

    def test_airbrush_brush_creates_valid_data(self):
        self._activate_brush("Airbrush")
        self._check_paint_stroke()

    def test_blend_hard_brush_creates_valid_data(self):
        self._activate_brush("Blend Hard")
        self._check_paint_stroke()

    def test_blend_soft_brush_creates_valid_data(self):
        self._activate_brush("Blend Soft")
        self._check_paint_stroke()

    def test_blend_square_brush_creates_valid_data(self):
        self._activate_brush("Blend Square")
        self._check_paint_stroke()

    def test_paint_blend_brush_creates_valid_data(self):
        self._activate_brush("Paint Blend")
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

    def test_paint_square_brush_creates_valid_data(self):
        self._activate_brush("Paint Square")
        self._check_paint_stroke()

    def test_sharpen_brush_creates_valid_data(self):
        self._activate_brush("Paint Hard")
        self._check_paint_stroke()

        self._activate_brush("Sharpen")
        self._check_paint_stroke()

    def test_smear_brush_creates_valid_data(self):
        self._activate_brush("Paint Hard")
        self._check_paint_stroke()

        self._activate_brush("Smear")
        self._check_paint_stroke()

    def test_blur_brush_creates_valid_data(self):
        self._activate_brush("Paint Hard")
        self._check_paint_stroke()

        self._activate_brush("Blur")
        self._check_paint_stroke()

    @unittest.skip("Brush requires raycast")
    def test_bend_boundary_cloth_brush_creates_valid_data(self):
        self._activate_brush("Bend Boundary Cloth")
        self._check_stroke()

    @unittest.skip("Brush requires raycast")
    def test_bend_twist_cloth_brush_creates_valid_data(self):
        self._activate_brush("Bend/Twist Cloth")
        self._check_stroke(start_over_mesh=True)

    def test_drag_cloth_brush_creates_valid_data(self):
        self._activate_brush("Drag Cloth")
        self._check_stroke(start_over_mesh=True)

    def test_expand_contract_cloth_brush_creates_valid_data(self):
        self._activate_brush("Expand/Contract Cloth")
        self._check_stroke(start_over_mesh=True)

    @unittest.skip("Brush requires raycast")
    def test_grab_cloth_brush_creates_valid_data(self):
        self._activate_brush("Grab Cloth")
        self._check_stroke(start_over_mesh=True)

    @unittest.skip("Brush has a typo currently in the name, 'Grab Planar Cloth '")
    def test_grab_planar_cloth_brush_creates_valid_data(self):
        self._activate_brush("Grab Planar Cloth")
        self._check_stroke(start_over_mesh=True)

    @unittest.skip("Brush requires raycast")
    def test_grab_random_cloth_brush_creates_valid_data(self):
        self._activate_brush("Grab Random Cloth")
        self._check_stroke(start_over_mesh=True)

    def test_inflate_cloth_brush_creates_valid_data(self):
        self._activate_brush("Inflate Cloth")
        self._check_stroke(start_over_mesh=True)

    def test_pinch_folds_cloth_brush_creates_valid_data(self):
        self._activate_brush("Pinch Folds Cloth")
        self._check_stroke(start_over_mesh=True)

    def test_pinch_point_cloth_brush_creates_valid_data(self):
        self._activate_brush("Pinch Point Cloth")
        self._check_stroke(start_over_mesh=True)

    def test_push_cloth_brush_creates_valid_data(self):
        self._activate_brush("Push Cloth")
        self._check_stroke(start_over_mesh=True)

    @unittest.skip("Brush requires raycast")
    def test_stretch_move_cloth_brush_creates_valid_data(self):
        self._activate_brush("Stretch/Move Cloth")
        self._check_stroke(start_over_mesh=True)

    @unittest.skip("Brush requires raycast")
    def test_twist_boundary_cloth_brush_creates_valid_data(self):
        self._activate_brush("Twist Boundary Cloth")
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
