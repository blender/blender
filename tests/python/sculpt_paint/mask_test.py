# SPDX-FileCopyrightText: 2025 Blender Authors
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
blender -b --factory-startup --python tests/python/mask_test.py -- --testdir tests/files/sculpting/
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


class GrowMaskTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "partially_masked_sphere.blend"), load_ui=False)
        bpy.ops.ed.undo_push()

    def test_grow_increases_number_of_masked_vertices(self):
        mesh = bpy.context.object.data
        mask_attr = mesh.attributes['.sculpt_mask']

        num_vertices = mesh.attributes.domain_size('POINT')

        old_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', old_mask_data)

        bpy.ops.sculpt.mask_filter(filter_type='GROW')

        new_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', new_mask_data)

        self.assertGreater(np.count_nonzero(new_mask_data), np.count_nonzero(old_mask_data))


class ShrinkMaskTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "partially_masked_sphere.blend"), load_ui=False)
        bpy.ops.ed.undo_push()

    def test_shrink_decreases_number_of_masked_vertices(self):
        mesh = bpy.context.object.data
        mask_attr = mesh.attributes['.sculpt_mask']

        num_vertices = mesh.attributes.domain_size('POINT')

        old_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', old_mask_data)

        bpy.ops.sculpt.mask_filter(filter_type='SHRINK')

        new_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', new_mask_data)

        self.assertLess(np.count_nonzero(new_mask_data), np.count_nonzero(old_mask_data))


class ClearMaskTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "partially_masked_sphere.blend"), load_ui=False)
        bpy.ops.ed.undo_push()

    def test_value_removes_attribute(self):
        mesh = bpy.context.object.data

        bpy.ops.paint.mask_flood_fill(mode='VALUE', value=0)

        self.assertFalse(mesh.attributes.get('.sculpt_mask'))


class InvertMaskTest(unittest.TestCase):
    def test_invert_applies_correct_values(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "partially_masked_sphere.blend"), load_ui=False)
        bpy.ops.ed.undo_push()

        mesh = bpy.context.object.data
        mask_attr = mesh.attributes['.sculpt_mask']

        num_vertices = mesh.attributes.domain_size('POINT')

        old_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', old_mask_data)

        bpy.ops.paint.mask_flood_fill(mode='INVERT')

        new_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', new_mask_data)

        expected_mask_data = np.array([1.0 - m for m in old_mask_data])

        self.assertEqual(expected_mask_data.tolist(), new_mask_data.tolist())

    def test_invert_on_empty_fills_mesh(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.ed.undo_push()

        bpy.ops.mesh.primitive_monkey_add()
        bpy.ops.sculpt.sculptmode_toggle()

        bpy.ops.paint.mask_flood_fill(mode='INVERT')

        mesh = bpy.context.object.data
        mask_attr = mesh.attributes['.sculpt_mask']

        num_vertices = mesh.attributes.domain_size('POINT')

        new_mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', new_mask_data)

        expected_mask_data = np.ones(num_vertices, dtype=np.float32)

        self.assertEqual(expected_mask_data.tolist(), new_mask_data.tolist())


class MaskByColorTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "plane_with_red_circle.blend"), load_ui=False)

        self.context_override = bpy.context.copy()
        set_view3d_context_override(self.context_override)
        bpy.ops.ed.undo_push()

    def test_off_grid_returns_cancelled(self):
        """Test that operator does not run when the cursor is not on the mesh."""

        with bpy.context.temp_override(**self.context_override):
            location = (0, 0)
            ret_val = bpy.ops.sculpt.mask_by_color(location=location)

            self.assertEqual({'CANCELLED'}, ret_val)

        mesh = bpy.context.object.data
        self.assertFalse('.sculpt_mask' in mesh.attributes.keys(), "Mesh should not have the .sculpt_mask attribute!")

    def test_on_circle_masks_red_vertices(self):
        """Test that the operator only masks red vertices on the mesh."""

        with bpy.context.temp_override(**self.context_override):
            location = (int(self.context_override['area'].width / 2), int(self.context_override['area'].height / 2))
            ret_val = bpy.ops.sculpt.mask_by_color(location=location)

            self.assertEqual({'FINISHED'}, ret_val)

        mesh = bpy.context.object.data
        color_attr = mesh.attributes['Color']
        mask_attr = mesh.attributes['.sculpt_mask']

        num_vertices = mesh.attributes.domain_size('POINT')

        color_data = np.zeros((num_vertices, 4), dtype=np.float32)
        color_attr.data.foreach_get('color', np.ravel(color_data))

        mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', mask_data)

        for i in range(num_vertices):
            # If either of the green or blue components are less than 1 (i.e. the vertex is the red part of the image instead of
            # the white background), then that vertex should also be masked.
            if color_data[i][1] < 0.4 and color_data[i][2] < 0.4:
                self.assertTrue(mask_data[i] > 0.0, f"Vertex {i} should be masked ({color_data[i]}) -> {mask_data[i]}")
            else:
                self.assertTrue(mask_data[i] < 0.1,
                                f"Vertex {i} should not be masked ({color_data[i]}) -> {mask_data[i]}")


class MaskFromCavityTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "plane_with_valley.blend"), load_ui=False)
        bpy.ops.ed.undo_push()

    def test_operator_masks_low_vertices(self):
        """Test that the operator applies a full mask value to any elements that are part of the cavity."""

        ret_val = bpy.ops.sculpt.mask_from_cavity()

        self.assertEqual({'FINISHED'}, ret_val)

        mesh = bpy.context.object.data
        position_attr = mesh.attributes['position']
        mask_attr = mesh.attributes['.sculpt_mask']

        num_vertices = mesh.attributes.domain_size('POINT')

        position_data = np.zeros((num_vertices, 3), dtype=np.float32)
        position_attr.data.foreach_get('vector', np.ravel(position_data))

        mask_data = np.zeros(num_vertices, dtype=np.float32)
        mask_attr.data.foreach_get('value', mask_data)

        for i in range(num_vertices):
            if position_data[i][2] < 0.0:
                self.assertEqual(
                    mask_data[i],
                    1.0,
                    f"Vertex {i} should be fully masked ({position_data[i]}) -> {mask_data[i]}")
            else:
                self.assertNotEqual(mask_data[i], 1.0,
                                    f"Vertex {i} should not be fully masked ({position_data[i]}) -> {mask_data[i]}")


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
