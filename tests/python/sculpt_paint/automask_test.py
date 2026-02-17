# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */

__all__ = (
    "main",
)

import os
import math
import unittest
import sys
import pathlib
import numpy as np

import bpy

"""
blender -b --factory-startup --python tests/python/sculpt_paint/sculpt_brushes_test.py -- --testdir tests/files/mesh_paint/
"""

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.test_helpers import AttributeType, BackendType, get_attribute_data, set_view3d_context_override, generate_stroke, generate_monkey

args = None


def get_verts_without_face_set(mesh, attr_data, face_set):
    faces = np.where(attr_data != face_set)
    verts_per_face = [list(mesh.polygons[int(idx)].vertices) for idx in faces[0]]
    verts = [v for face_verts in verts_per_face for v in face_verts]

    return list(set(verts))


def get_verts_with_face_set(mesh, attr_data, face_set):
    faces = np.where(attr_data == face_set)
    verts_per_face = [list(mesh.polygons[int(idx)].vertices) for idx in faces[0]]
    verts = [v for face_verts in verts_per_face for v in face_verts]

    return list(set(verts))


def get_verts_with_island_id(attr_data, island_id):
    verts = np.where(attr_data == island_id)

    return verts


def get_verts_without_island_id(attr_data, island_id):
    verts = np.where(attr_data != island_id)

    return verts


class BrushAutomaskTest(unittest.TestCase):
    """
    Test that automasking prevents certain vertices from being modified
    """

    def setUp(self):
        bpy.ops.wm.open_mainfile(
            filepath=str(
                args.testdir /
                "monkey_realized_island_id_and_face_set.blend"),
            load_ui=False)
        bpy.ops.object.mode_set(mode="SCULPT")
        bpy.ops.ed.undo_push()

        result = bpy.ops.brush.asset_activate(
            asset_library_type='ESSENTIALS',
            relative_asset_identifier='brushes/essentials_brushes-mesh_sculpt.blend/Brush/Draw')
        self.assertEqual({'FINISHED'}, result)

    def test_face_set_automasking_ignores_any_non_starting_face_set(self):
        # This test mesh has 3 face sets, 1 and 2 are used for the eyes, the rest of the monkey's head is 3
        ACTIVE_FACE_SET = 3
        bpy.data.scenes[0].tool_settings.sculpt.use_automasking_face_sets = True

        initial_data = get_attribute_data(BackendType.MESH, AttributeType.POSITION)

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.sculpt.brush_stroke(
                stroke=generate_stroke(
                    context_override,
                    start_percent=(0.5, 0.5)),
                override_location=True)

        new_data = get_attribute_data(BackendType.MESH, AttributeType.POSITION)

        mesh = bpy.context.active_object.data
        num_faces = mesh.attributes.domain_size('FACE')
        face_set_data = np.zeros(num_faces, dtype=np.int32)
        face_set_attribute = mesh.attributes.get(".sculpt_face_set")
        face_set_attribute.data.foreach_get('value', np.ravel(face_set_data))

        verts_with_face_set = get_verts_with_face_set(mesh, face_set_data, ACTIVE_FACE_SET)

        filtered_initial_data = initial_data[verts_with_face_set]
        filtered_new_data = new_data[verts_with_face_set]

        any_different = any([orig != new for (orig, new) in zip(filtered_initial_data, filtered_new_data)])
        self.assertTrue(any_different, "At least one position should be different from its original value")

        verts_without_face_set = get_verts_without_face_set(mesh, face_set_data, ACTIVE_FACE_SET)

        filtered_initial_data = initial_data[verts_without_face_set]
        filtered_new_data = new_data[verts_without_face_set]

        all_same = all([orig == new for (orig, new) in zip(filtered_initial_data, filtered_new_data)])
        self.assertTrue(all_same, "Vertices that are not included in the original face sets should be unchanged")

    def test_topology_automasking_ignores_any_non_starting_island(self):
        # This test mesh has 3 island ids, 0 and 1 are used for the eyes, the rest of the monkey's head is 2
        ACTIVE_ISLAND_ID = 2
        bpy.data.scenes[0].tool_settings.sculpt.use_automasking_face_sets = True

        initial_data = get_attribute_data(BackendType.MESH, AttributeType.POSITION)

        context_override = bpy.context.copy()
        set_view3d_context_override(context_override)
        with bpy.context.temp_override(**context_override):
            bpy.ops.sculpt.brush_stroke(
                stroke=generate_stroke(
                    context_override,
                    start_percent=(0.5, 0.5)),
                override_location=True)

        new_data = get_attribute_data(BackendType.MESH, AttributeType.POSITION)

        mesh = bpy.context.active_object.data
        num_faces = mesh.attributes.domain_size('POINT')
        island_id_data = np.zeros(num_faces, dtype=np.int32)
        island_id_attribute = mesh.attributes.get("island_id")
        island_id_attribute.data.foreach_get('value', np.ravel(island_id_data))

        verts_with_island_id = get_verts_with_island_id(island_id_data, ACTIVE_ISLAND_ID)

        filtered_initial_data = initial_data[verts_with_island_id]
        filtered_new_data = new_data[verts_with_island_id]

        any_different = any([orig != new for (orig, new) in zip(filtered_initial_data, filtered_new_data)])
        self.assertTrue(any_different, "At least one position should be different from its original value")

        verts_without_island_id = get_verts_without_island_id(island_id_data, ACTIVE_ISLAND_ID)

        filtered_initial_data = initial_data[verts_without_island_id]
        filtered_new_data = new_data[verts_without_island_id]

        all_same = all([orig == new for (orig, new) in zip(filtered_initial_data, filtered_new_data)])
        self.assertTrue(all_same, "Vertices that are not part of the initial mesh island should be unchanged")


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
