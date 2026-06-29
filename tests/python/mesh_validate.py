# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


import unittest
import bpy
import sys
import pathlib

"""
blender -b --factory-startup --python tests/python/mesh_validate.py -- --testdir tests/files/
"""


class TestMeshValidate(unittest.TestCase):

    def setUp(self):
        if bpy.context.object and bpy.context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()

        for mesh in list(bpy.data.meshes):
            bpy.data.meshes.remove(mesh)

    def tearDown(self):
        for mesh in list(bpy.data.meshes):
            bpy.data.meshes.remove(mesh)

    def test_invalid_edge_vertex_indices(self):
        verts = [(0, 0, 0), (1, 1, 1)]
        edges = [(0, 99)]  # Invalid vertex 99

        mesh = bpy.data.meshes.new("test_mesh")
        mesh.from_pydata(verts, edges, [])

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_duplicate_edge_vertex_indices(self):
        verts = [(0, 0, 0), (1, 1, 1)]
        edges = [(0, 0)]  # Invalid edge from a vertex to itself

        mesh = bpy.data.meshes.new("test_mesh")
        mesh.from_pydata(verts, edges, [])

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_bad_face_offsets(self):
        bpy.ops.mesh.primitive_cube_add()
        mesh = bpy.context.active_object.data

        mesh.polygons[0].loop_start = 100

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_bad_material_indices(self):
        bpy.ops.mesh.primitive_plane_add()
        obj = bpy.context.active_object
        mesh = obj.data

        attr = mesh.attributes.new(name="material_index", type='INT', domain='FACE')
        attr.data[0].value = -4

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_duplicate_faces(self):
        verts = [(0, 0, 0), (1, 0, 0), (1, 1, 0)]
        faces = [(0, 1, 2), (2, 0, 1)]

        mesh = bpy.data.meshes.new("test_mesh")
        mesh.from_pydata(verts, [], faces)

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_invalid_float_attributes(self):
        bpy.ops.mesh.primitive_plane_add()
        mesh = bpy.context.active_object.data

        mesh.vertices[0].co.x = float('nan')

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_duplicate_edges(self):
        verts = [(0, 0, 0), (1, 1, 1)]
        edges = [(0, 1), (1, 0)]

        mesh = bpy.data.meshes.new("test_mesh")
        mesh.from_pydata(verts, edges, [])

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_faces_with_bad_edge_references(self):
        verts = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)]
        edges = []
        faces = [(0, 1, 2, 3)]

        mesh = bpy.data.meshes.new("test_mesh")
        mesh.from_pydata(verts, edges, faces)

        corner_edges = mesh.attributes[".corner_edge"].data
        corner_edges[2].value = 0

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_negative_intermediate_face_offset(self):
        # An intermediate face's start offset is negative.
        verts = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0),
                 (2, 0, 0), (3, 0, 0), (3, 1, 0), (2, 1, 0)]
        edges = []
        faces = [(0, 1, 2, 3), (4, 5, 6, 7)]

        mesh = bpy.data.meshes.new("test_mesh")
        mesh.from_pydata(verts, edges, faces)

        mesh.polygons[1].loop_start = -4

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_intermediate_face_offset_past_corners(self):
        # An intermediate face's start offset is beyond the number of corners.
        verts = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0),
                 (2, 0, 0), (3, 0, 0), (3, 1, 0), (2, 1, 0)]
        edges = []
        faces = [(0, 1, 2, 3), (4, 5, 6, 7)]

        mesh = bpy.data.meshes.new("test_mesh")
        mesh.from_pydata(verts, edges, faces)

        mesh.polygons[1].loop_start = 1000

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_corner_edge_to_duplicate_edge(self):
        # A corner referencing a duplicate edge that gets removed must be
        # redirected to the kept edge.
        verts = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)]
        edges = [(0, 1), (1, 2), (2, 3), (3, 0), (0, 1)]
        faces = [(0, 1, 2, 3)]

        mesh = bpy.data.meshes.new("test_mesh")
        mesh.from_pydata(verts, edges, faces)

        # Point the first corner's edge at the duplicate.
        corner_edges = mesh.attributes[".corner_edge"].data
        corner_edges[0].value = 4

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_bad_corner_edge_after_bad_vert_edge(self):
        # When earlier edges have been removed (e.g. for out-of-range verts), the
        # index used by the bad-edge fix-up must adapt.
        verts = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)]
        faces = [(0, 1, 2, 3)]

        mesh = bpy.data.meshes.new("test_mesh")
        mesh.from_pydata(verts, [], faces)

        # Give an edge an out-of-range vertex so it gets removed by validation.
        mesh.attributes[".edge_verts"].data[0].value = (0, 99)

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_missing_edge_with_oob_corner_edge(self):
        # A face with a missing edge and an out-of-range corner_edge index.
        verts = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)]
        faces = [(0, 1, 2, 3)]

        mesh = bpy.data.meshes.new("test_mesh")
        mesh.from_pydata(verts, [], faces)

        mesh.attributes[".edge_verts"].data[0].value = (0, 99)
        mesh.attributes[".corner_edge"].data[0].value = 999

        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))

    def test_overlapping_face_offsets(self):
        # Non-monotonic face offsets.
        verts = [(float(i), 0.0, 0.0) for i in range(12)]
        faces = [(0, 1, 2, 3), (4, 5, 6, 7), (8, 9, 10, 11)]

        mesh = bpy.data.meshes.new("test_mesh")
        mesh.from_pydata(verts, [], faces)

        mesh.polygons[1].loop_start = 5
        mesh.polygons[2].loop_start = 3

        self.assertTrue(mesh.validate(verbose=True))
        self.assertEqual(len(mesh.polygons), 0)
        self.assertFalse(mesh.validate(verbose=True))


args = None


class TestMeshValidateExistingFile(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "sculpting/invalid_mdisp_cube.blend"), load_ui=False)

    def test_mesh_with_bad_multires_displacements(self):
        mesh = bpy.context.active_object.data
        self.assertTrue(mesh.validate(verbose=True))
        self.assertFalse(mesh.validate(verbose=True))


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
