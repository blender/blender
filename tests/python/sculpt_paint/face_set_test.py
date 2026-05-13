# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */
"""
blender -b --factory-startup --python tests/python/sculpt_paint/face_set_test.py -- --testdir tests/files/sculpting
"""

__all__ = (
    "main",
)

import numpy as np
import os
import pathlib
import sys
import unittest

import bpy

args = None


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


class ChangeVisibilityTest(unittest.TestCase):
    """
    Test that none of the mesh filters create NaN or inf valued vertices
    """

    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "30k_monkey_mask_and_face_set.blend"), load_ui=False)
        bpy.ops.ed.undo_push()
        bpy.ops.sculpt.sculptmode_toggle()

    def test_toggle_with_no_face_set_hides_and_unhides_everything(self):
        bpy.ops.sculpt.face_set_change_visibility(mode='TOGGLE', active_face_set=0)

        hidden_faces = get_attribute_data(
            attribute_name=".hide_poly",
            attribute_domain='FACE',
            attribute_size=1,
            attribute_type=np.bool)

        mesh = bpy.context.object.data
        faces_num = mesh.attributes.domain_size('FACE')

        self.assertEqual(np.count_nonzero(hidden_faces), faces_num, "All faces should be hidden")

        bpy.ops.sculpt.face_set_change_visibility(mode='TOGGLE', active_face_set=0)

        hidden_faces = get_attribute_data(
            attribute_name=".hide_poly",
            attribute_domain='FACE',
            attribute_size=1,
            attribute_type=np.bool)
        self.assertEqual(np.count_nonzero(hidden_faces), 0, "No faces should be hidden")

    def test_toggle_with_specified_face_set_modifies_other_faces(self):
        face_set_data = get_attribute_data(
            attribute_name=".sculpt_face_set",
            attribute_domain='FACE',
            attribute_size=1,
            attribute_type=np.int32)

        bpy.ops.sculpt.face_set_change_visibility(mode='TOGGLE', active_face_set=2)

        hidden_faces = get_attribute_data(
            attribute_name=".hide_poly",
            attribute_domain='FACE',
            attribute_size=1,
            attribute_type=np.bool)

        self.assertEqual(np.count_nonzero(hidden_faces), np.count_nonzero(
            face_set_data != 2), "All non-specified faces should be hidden")

        bpy.ops.sculpt.face_set_change_visibility(mode='TOGGLE', active_face_set=2)

        hidden_faces = get_attribute_data(
            attribute_name=".hide_poly",
            attribute_domain='FACE',
            attribute_size=1,
            attribute_type=np.bool)
        self.assertEqual(np.count_nonzero(hidden_faces), 0, "No faces should be hidden")

    def test_hide_show_affects_specified_faces(self):
        face_set_data = get_attribute_data(
            attribute_name=".sculpt_face_set",
            attribute_domain='FACE',
            attribute_size=1,
            attribute_type=np.int32)

        bpy.ops.sculpt.face_set_change_visibility(mode='HIDE_ACTIVE', active_face_set=2)

        hidden_faces = get_attribute_data(
            attribute_name=".hide_poly",
            attribute_domain='FACE',
            attribute_size=1,
            attribute_type=np.bool)

        self.assertEqual(
            np.count_nonzero(hidden_faces),
            np.count_nonzero(
                face_set_data == 2),
            "All specified faces should be hidden")

        bpy.ops.sculpt.face_set_change_visibility(mode='SHOW_ACTIVE', active_face_set=2)

        hidden_faces = get_attribute_data(
            attribute_name=".hide_poly",
            attribute_domain='FACE',
            attribute_size=1,
            attribute_type=np.bool)
        self.assertEqual(np.count_nonzero(hidden_faces), 0, "No faces should be hidden")


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
