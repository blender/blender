# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


import unittest
import bpy
import sys


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


if __name__ == '__main__':
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
