# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --python tests/python/bl_pyapi_text.py -- --verbose
import bpy
import unittest


class TestMeshSpatialOrganization(unittest.TestCase):

    def setUp(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete(use_global=False)
        if bpy.context.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

    def tearDown(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete(use_global=False)

    def create_subdivided_plane(self, subdivisions):
        bpy.ops.mesh.primitive_plane_add(size=2, location=(0, 0, 0))
        plane = bpy.context.active_object
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.subdivide(number_cuts=subdivisions, smoothness=0.0)
        bpy.ops.object.mode_set(mode='OBJECT')
        return plane

    def get_vertex_data(self, obj):
        mesh = obj.data
        vertices = [(v.co.x, v.co.y, v.co.z) for v in mesh.vertices]
        return {
            'vertices': vertices,
            'vertex_count': len(vertices)
        }

    def create_reference_mesh(self, source_obj):
        """Create a reference copy of the mesh for comparison"""
        bpy.context.view_layer.objects.active = source_obj
        source_obj.select_set(True)
        bpy.ops.object.duplicate()
        reference_obj = bpy.context.active_object
        reference_obj.name = "reference_mesh"
        return reference_obj

    def test_spatial_organization_changes_vertex_order(self):
        plane = self.create_subdivided_plane(subdivisions=50)
        initial_data = self.get_vertex_data(plane)
        bpy.ops.mesh.reorder_vertices_spatial()
        final_data = self.get_vertex_data(plane)
        self.assertEqual(initial_data['vertex_count'], final_data['vertex_count'])
        vertices_changed = initial_data['vertices'] != final_data['vertices']
        self.assertTrue(vertices_changed)

    def test_spatial_organization_preserves_topology(self):
        plane = self.create_subdivided_plane(subdivisions=50)
        reference_plane = self.create_reference_mesh(plane)
        bpy.ops.mesh.reorder_vertices_spatial()

        comparison_result = plane.data.unit_test_compare(mesh=reference_plane.data)
        self.assertEqual(comparison_result, "The geometries are the same up to a change of indices")


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
