# SPDX-FileCopyrightText: 2026 Blender Authors
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
blender -b --factory-startup --python tests/python/sculpt_paint/mesh_filter_test.py -- --testdir tests/files/
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


class MeshFilterTests(unittest.TestCase):
    """
    Test that none of the mesh filters create NaN or inf valued vertices
    """

    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "30k_monkey_mask_and_face_set.blend"), load_ui=False)
        bpy.ops.ed.undo_push()
        bpy.ops.sculpt.sculptmode_toggle()

    def _check_filter(self, type, opts={}):
        # Ideally, we would use something like pytest and parameterized tests here, but this helper function is an
        # alright solution for now...

        initial_data = get_attribute_data()

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.sculpt.mesh_filter(type=type, **opts)

        new_data = get_attribute_data()

        # Note, depending on if the tests are run with asserts enabled or not, the test may fail before this point
        # inside blender itself.
        all_valid = all([not math.isinf(pos) and not math.isnan(pos) for pos in new_data])
        any_different = any([orig != new for (orig, new) in zip(initial_data, new_data)])
        self.assertTrue(all_valid, "All position components should be rational values")
        self.assertTrue(any_different, "At least one position should be different from its original value")

    def test_smooth_filter_creates_valid_data(self):
        self._check_filter('SMOOTH')

    def test_surface_smooth_filter_creates_valid_data(self):
        self._check_filter('SURFACE_SMOOTH')

    def test_inflate_filter_creates_valid_data(self):
        self._check_filter('INFLATE')

    def test_relax_topology_filter_creates_valid_data(self):
        self._check_filter('RELAX')

    def test_relax_face_sets_filter_creates_valid_data(self):
        self._check_filter('RELAX_FACE_SETS')

    def test_sharpen_filter_creates_valid_data(self):
        self._check_filter('SHARPEN')

    def test_sharpen_filter_intensify_details_creates_valid_data(self):
        self._check_filter('SHARPEN', opts={"sharpen_intensify_detail_strength": 1.0})

    def test_sharpen_filter_curvature_smooth_creates_valid_data(self):
        self._check_filter('SHARPEN', opts={"sharpen_curvature_smooth_iterations": 10})

    def test_enhance_details_filter_creates_valid_data(self):
        self._check_filter('ENHANCE_DETAILS')

    def test_scale_filter_creates_valid_data(self):
        self._check_filter('SCALE')

    def test_sphere_filter_creates_valid_data(self):
        self._check_filter('SPHERE')

    def test_randomize_filter_creates_valid_data(self):
        self._check_filter('RANDOM')


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


if __name__ == '__main__':
    main()
