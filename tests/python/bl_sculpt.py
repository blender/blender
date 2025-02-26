import unittest
import sys
import pathlib
import numpy as np

import bpy

"""
blender -b --factory-startup --python tests/python/bl_sculpt.py -- --testdir tests/data/sculpting/
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


class MaskByColorTest(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "plane_with_red_circle.blend"), load_ui=False)

        self.context_override = bpy.context.copy()
        set_view3d_context_override(self.context_override)
        bpy.ops.ed.undo_push()

    def test_off_grid_returns_cancelled(self):
        with bpy.context.temp_override(**self.context_override):
            location = (0, 0)
            ret_val = bpy.ops.sculpt.mask_by_color(location=location)

            self.assertEqual({'CANCELLED'}, ret_val)

        mesh = bpy.context.object.data
        self.assertFalse('.sculpt_mask' in mesh.attributes.keys(), "Mesh should not have the .sculpt_mask attribute!")

    def test_on_circle_masks_red_vertices(self):
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
