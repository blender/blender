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

from enum import Enum, unique


@unique
class BackendType(Enum):
    MESH = 0
    MULTIRES = 1


@unique
class AttributeType(Enum):
    POSITION = 0
    MASK = 1
    FACE_SET = 2
    COLOR = 3


COLOR_BACKEND_TYPES = [BackendType.MESH]

# Applying a multires mesh does not transfer mask values.
# See #153743 for a tracking issue to enable these tests
MASK_BACKEND_TYPES = [BackendType.MESH]


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

    num_steps = 50
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


def get_mesh(backend_type):
    if backend_type == BackendType.MESH:
        return bpy.context.active_object.data
    else:
        # Duplicate and apply the modifier
        bpy.ops.sculpt.sculptmode_toggle()

        original_object = bpy.data.objects['Suzanne']
        bpy.ops.object.select_all(action='DESELECT')
        original_object.select_set(True)

        bpy.ops.object.duplicate()

        duplicate_object = bpy.context.selected_objects[0]
        bpy.context.view_layer.objects.active = bpy.context.selected_objects[0]

        bpy.context.active_object.modifiers['Multires'].levels = 1

        bpy.ops.object.modifier_apply(modifier='Multires')

        # Restore initial object as "active" and return to sculpt mode
        bpy.context.view_layer.objects.active = original_object
        bpy.ops.sculpt.sculptmode_toggle()

        return duplicate_object.data


def get_attribute_data(backend_type, attribute_type):
    mesh = get_mesh(backend_type)

    match attribute_type:
        case AttributeType.POSITION:
            attribute_name = 'position'
            attribute_domain = 'POINT'
            attribute_size = 3
            attribute_type = np.float32
            is_color = False
        case AttributeType.MASK:
            attribute_name = '.sculpt_mask'
            attribute_domain = 'POINT'
            attribute_size = 1
            attribute_type = np.float32
            is_color = False
        case AttributeType.FACE_SET:
            attribute_name = '.sculpt_face_set'
            attribute_domain = 'FACE'
            attribute_size = 1
            attribute_type = np.int32
            is_color = False
        case AttributeType.COLOR:
            attribute_name = 'Color'
            attribute_domain = 'POINT'
            attribute_size = 4
            attribute_type = np.float32
            is_color = True
        case _:
            raise Exception("Invalid type specified")

    if attribute_type == AttributeType.COLOR and backend_type == BackendType.MULTIRES:
        raise Exception("Multires does not support color attributes")

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

    def _initialize(self, backend):
        """
        Reset the file to the initial working state, unfortunately `setUp` does not work with subTest if using the
        latter as parameterized tests.
        """
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.mesh.primitive_monkey_add()

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.view3d.view_axis(type='FRONT')
            bpy.ops.view3d.view_selected()

        if backend == BackendType.MESH:
            bpy.ops.object.subdivision_set(level=2, relative=False, ensure_modifier=True)

        bpy.ops.ed.undo_push()
        bpy.ops.sculpt.sculptmode_toggle()

        if backend == BackendType.MULTIRES:
            bpy.ops.object.subdivision_set(level=2, relative=False, ensure_modifier=True)

    def _activate_brush(self, brush):
        """
        Activate a specified brush
        """
        result = bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_sculpt.blend/Brush/{}'.format(brush))
        self.assertEqual({'FINISHED'}, result)

    def _check_stroke(self, backend, attribute, *, start_over_mesh=False, opts={}):
        """
        Compare the prior and post states of a brush stroke
        """
        initial_data = get_attribute_data(backend, attribute)

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.sculpt.brush_stroke(
                stroke=generate_stroke(
                    context_override,
                    start_over_mesh),
                override_location=True, **opts)

        new_data = get_attribute_data(backend, attribute)

        # Note, depending on if the tests are run with asserts enabled or not, the test may fail before this point
        # inside blender itself.
        match attribute:
            case AttributeType.POSITION:
                all_valid = all([not math.isinf(pos) and not math.isnan(pos) for pos in new_data])
                any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
                self.assertTrue(all_valid, "All position components should be rational values")
                self.assertTrue(any_different, "At least one position should be different from its original value")
            case AttributeType.MASK:
                all_valid = all([not math.isinf(mask) and not math.isnan(mask) for mask in new_data])
                any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
                self.assertTrue(all_valid, "All mask values should be rational values")
                self.assertTrue(any_different, "At least one mask should be different from its original value")
            case AttributeType.FACE_SET:
                all_valid = all([face_set_id == 1 or face_set_id == 2 for face_set_id in new_data])
                any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
                self.assertTrue(all_valid, "All face set values should be 1 or 2 valued")
                self.assertTrue(any_different, "At least one face set should be different from its original value")
            case AttributeType.COLOR:
                all_valid = all([not math.isinf(channel) and not math.isnan(channel) for channel in new_data])
                any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
                self.assertTrue(all_valid, "All color components should be rational values")
                self.assertTrue(any_different,
                                "At least one color component should be different from its original value")
            case _:
                raise Exception("Invalid attribute type")

    def test_blob_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Blob")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_clay_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Clay")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_clay_strips_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Clay Strips")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_clay_thumb_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Clay Thumb")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_crease_polish_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Crease Polish")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_crease_sharp_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Crease Sharp")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_draw_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Draw")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_draw_sharp_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Draw Sharp")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_inflate_deflate_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Inflate/Deflate")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_fill_deepen_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Fill/Deepen")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_flatten_contrast_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Flatten/Contrast")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_plateau_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Plateau")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_scrape_multiplane_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Scrape Multiplane")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_scrape_fill_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Scrape/Fill")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_smooth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Smooth")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_smooth_brush_invert_mode_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Smooth")
                self._check_stroke(backend, AttributeType.POSITION, opts={"mode": 'INVERT'})

    def test_trim_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Trim")
                self._check_stroke(backend, AttributeType.POSITION)

    @unittest.skip("Needs raycast")
    def test_boundary_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Boundary")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_elastic_grab_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Elastic Grab")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_elastic_snake_hook_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Elastic Snake Hook")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_grab_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Grab")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_grab_2d_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Grab 2D")
                self._check_stroke(backend, AttributeType.POSITION)

    @unittest.skip("Requires raycast")
    def test_grab_silhouette_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Grab Silhouette")
                self._check_stroke(backend, AttributeType.POSITION, start_over_mesh=True)

    def test_nudge_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Nudge")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_pinch_magnify_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Pinch/Magnify")
                self._check_stroke(backend, AttributeType.POSITION)

    @unittest.skip("Brush requires raycast")
    def test_pose_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Pose")
                self._check_stroke(backend, AttributeType.POSITION, start_over_mesh=True)

    def test_pull_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Pull")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_relax_pinch_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Relax Pinch")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_relax_slide_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Relax Slide")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_relax_brush_smooth_mode_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Relax Slide")
                self._check_stroke(backend, AttributeType.POSITION, opts={"brush_toggle": 'SMOOTH'})

    def test_snake_hook_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Snake Hook")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_thumb_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Thumb")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_twist_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Twist")
                self._check_stroke(backend, AttributeType.POSITION, start_over_mesh=True)

    def test_mask_brush_creates_valid_data(self):
        for backend in MASK_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Mask")
                self._check_stroke(backend, AttributeType.MASK)

    def test_mask_brush_smooth_mode_creates_valid_data(self):
        for backend in MASK_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Mask")
                self._check_stroke(backend, AttributeType.MASK)

                self._check_stroke(backend, AttributeType.MASK, opts={"brush_toggle": 'SMOOTH'})

    def test_face_set_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Face Set Paint")
                self._check_stroke(backend, AttributeType.FACE_SET)

    def test_face_set_brush_smooth_mode_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Face Set Paint")
                self._check_stroke(backend, AttributeType.FACE_SET)

                self._check_stroke(backend, AttributeType.POSITION, opts={"brush_toggle": 'SMOOTH'})

    def test_airbrush_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Airbrush")
                self._check_stroke(backend, AttributeType.COLOR)

    def test_blend_hard_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Blend Hard")
                self._check_stroke(backend, AttributeType.COLOR)

    def test_blend_soft_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Blend Soft")
                self._check_stroke(backend, AttributeType.COLOR)

    def test_blend_square_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Blend Square")
                self._check_stroke(backend, AttributeType.COLOR)

    def test_paint_blend_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Paint Blend")
                self._check_stroke(backend, AttributeType.COLOR)

    def test_paint_hard_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Paint Hard")
                self._check_stroke(backend, AttributeType.COLOR)

    def test_paint_hard_pressure_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Paint Hard Pressure")
                self._check_stroke(backend, AttributeType.COLOR)

    def test_paint_soft_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Paint Soft")
                self._check_stroke(backend, AttributeType.COLOR)

    def test_paint_soft_pressure_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Paint Soft Pressure")
                self._check_stroke(backend, AttributeType.COLOR)

    def test_paint_square_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Paint Square")
                self._check_stroke(backend, AttributeType.COLOR)

    def test_sharpen_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Paint Hard")
                self._check_stroke(backend, AttributeType.COLOR)

                self._activate_brush("Sharpen")
                self._check_stroke(backend, AttributeType.COLOR)

    def test_smear_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Paint Hard")
                self._check_stroke(backend, AttributeType.COLOR)

                self._activate_brush("Smear")
                self._check_stroke(backend, AttributeType.COLOR)

    def test_blur_brush_creates_valid_data(self):
        for backend in COLOR_BACKEND_TYPES:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Paint Hard")
                self._check_stroke(backend, AttributeType.COLOR)

                self._activate_brush("Blur")
                self._check_stroke(backend, AttributeType.COLOR)

    @unittest.skip("Brush requires raycast")
    def test_bend_boundary_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Bend Boundary Cloth")
                self._check_stroke(backend, AttributeType.POSITION)

    @unittest.skip("Brush requires raycast")
    def test_bend_twist_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Bend/Twist Cloth")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_drag_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Drag Cloth")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_expand_contract_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Expand/Contract Cloth")
                self._check_stroke(backend, AttributeType.POSITION)

    @unittest.skip("Brush requires raycast")
    def test_grab_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Grab Cloth")
                self._check_stroke(backend, AttributeType.POSITION)

    @unittest.skip("Brush has a typo currently in the name, 'Grab Planar Cloth '")
    def test_grab_planar_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Grab Planar Cloth")
                self._check_stroke(backend, AttributeType.POSITION, start_over_mesh=True)

    @unittest.skip("Brush requires raycast")
    def test_grab_random_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Grab Random Cloth")
                self._check_stroke(backend, AttributeType.POSITION, start_over_mesh=True)

    def test_inflate_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Inflate Cloth")
                self._check_stroke(backend, AttributeType.POSITION, start_over_mesh=True)

    def test_pinch_folds_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Pinch Folds Cloth")
                self._check_stroke(backend, AttributeType.POSITION, start_over_mesh=True)

    def test_pinch_point_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Pinch Point Cloth")
                self._check_stroke(backend, AttributeType.POSITION, start_over_mesh=True)

    def test_push_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Push Cloth")
                self._check_stroke(backend, AttributeType.POSITION, start_over_mesh=True)

    @unittest.skip("Brush requires raycast")
    def test_stretch_move_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Stretch/Move Cloth")
                self._check_stroke(backend, AttributeType.POSITION, start_over_mesh=True)

    @unittest.skip("Brush requires raycast")
    def test_twist_boundary_cloth_brush_creates_valid_data(self):
        for backend in BackendType:
            with self.subTest(backend):
                self._initialize(backend)
                self._activate_brush("Twist Boundary Cloth")
                self._check_stroke(backend, AttributeType.POSITION)

    def test_multires_smear_brush_creates_valid_data(self):
        self._initialize(BackendType.MULTIRES)
        self._activate_brush("Draw")
        self._check_stroke(BackendType.MULTIRES, AttributeType.POSITION)

        self._activate_brush("Smear Multires Displacement")
        self._check_stroke(BackendType.MULTIRES, AttributeType.POSITION)

    def test_multires_erase_brush_creates_valid_data(self):
        self._initialize(BackendType.MULTIRES)
        self._activate_brush("Draw")
        self._check_stroke(BackendType.MULTIRES, AttributeType.POSITION)

        self._activate_brush("Erase Multires Displacement")
        self._check_stroke(BackendType.MULTIRES, AttributeType.POSITION)


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
