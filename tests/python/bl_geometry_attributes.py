# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --python tests/python/bl_geometry_attributes.py -- --verbose
import bpy
import unittest


class TestCurves(unittest.TestCase):
    def setUp(self):
        self.curves = bpy.data.hair_curves.new("test")
        # 50 points, 4 curves
        self.curves.add_curves([5, 10, 15, 20])

    def tearDown(self):
        bpy.data.hair_curves.remove(self.curves)
        del self.curves

    def test_add_attribute(self):
        a = self.curves.attributes.new("a", 'FLOAT', 'POINT')
        self.assertTrue(a.name == "a")
        self.assertTrue(a.data_type == 'FLOAT')
        self.assertTrue(a.domain == 'POINT')
        self.assertTrue(a.storage_type == 'ARRAY')
        self.assertFalse(a.is_internal)
        self.assertTrue(len(a.data) == 50)

    def test_is_required(self):
        a = self.curves.attributes.new("a", 'FLOAT', 'POINT')
        self.assertFalse(a.is_required)
        self.assertTrue(self.curves.attributes["position"].is_required)

    def test_pointer_stability_on_add(self):
        attrs = [self.curves.attributes.new("a" + str(i), 'FLOAT', 'POINT') for i in range(100)]
        for i in range(100):
            self.assertTrue(attrs[i].name == "a" + str(i))
            self.assertTrue(attrs[i].data_type == 'FLOAT')
            self.assertTrue(attrs[i].domain == 'POINT')

        # Remove some attributes
        for i in range(50):
            self.curves.attributes.remove(attrs[i])
            del attrs[i]

        self.assertTrue(len(self.curves.attributes) == 51)
        self.assertTrue(self.curves.attributes["a51"].name == "a51")

    def test_add_same_name(self):
        a = self.curves.attributes.new("a", 'FLOAT', 'POINT')
        b = self.curves.attributes.new("a", 'BOOLEAN', 'CURVE')
        self.assertFalse(a.name == b.name)

    def test_add_wrong_domain(self):
        with self.assertRaises(RuntimeError):
            self.curves.attributes.new("a", 'FLOAT', 'CORNER')

    def rename_attribute(self, name, new_name):
        with self.assertRaises(RuntimeError):
            self.curves.attributes["position"].name = "asjhfksjhdfkjsh"
        a = self.curves.attributes.new("a", 'FLOAT', 'POINT')
        a.name = "better_name"
        self.assertTrue(a.name == "better_name")
        self.assertTrue(self.curves.attributes["better_name"].name == "better_name")

    def test_long_name(self):
        self.curves.attributes.new("a" * 100, 'FLOAT', 'POINT')
        self.assertTrue(self.curves.attributes["a" * 100].name == "a" * 100)


class TestMesh(unittest.TestCase):
    def setUp(self):
        self.mesh = bpy.data.meshes.new("test")
        self.mesh.vertices.add(10)

    def tearDown(self):
        bpy.data.meshes.remove(self.mesh)
        del self.mesh

    def test_set_active_color_attribute_if_empty(self):
        self.assertTrue(self.mesh.attributes.active_color_name == "")
        self.assertTrue(self.mesh.attributes.default_color_name == "")

        self.mesh.attributes.new("a", 'FLOAT_COLOR', 'POINT')
        self.assertTrue(self.mesh.attributes.active_color_name == "a")
        self.assertTrue(self.mesh.attributes.default_color_name == "a")

        self.mesh.attributes.new("b", 'FLOAT_COLOR', 'POINT')
        self.assertTrue(self.mesh.attributes.active_color_name == "a")
        self.assertTrue(self.mesh.attributes.default_color_name == "a")

    def test_set_active_uv_map_if_empty(self):
        self.assertTrue(self.mesh.uv_layers.active is None)

        self.mesh.attributes.new("a", 'FLOAT2', 'CORNER')
        self.assertTrue(self.mesh.uv_layers.active is not None)
        self.assertTrue(self.mesh.uv_layers.active.name == "a")

        self.mesh.attributes.new("b", 'FLOAT2', 'CORNER')
        self.assertTrue(self.mesh.uv_layers.active is not None)
        self.assertTrue(self.mesh.uv_layers.active.name == "a")


class MeshObjectTest(unittest.TestCase):
    def setUp(self):
        self.mesh = bpy.data.meshes.new("test")
        self.mesh.from_pydata([(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)], [], [(0, 1, 2, 3)])
        self.obj = bpy.data.objects.new("test", self.mesh)
        bpy.context.scene.collection.objects.link(self.obj)
        bpy.context.view_layer.objects.active = self.obj

    def tearDown(self):
        bpy.data.objects.remove(self.obj)
        bpy.data.meshes.remove(self.mesh)
        del self.obj
        del self.mesh


class TestMeshVertexGroupNameClash(MeshObjectTest):
    def test_add_attribute(self):
        self.obj.vertex_groups.new(name="UVMap")
        attribute = self.mesh.attributes.new("UVMap", 'FLOAT2', 'CORNER')
        self.assertFalse(attribute.name == "UVMap")
        self.assertTrue("UVMap" in [group.name for group in self.obj.vertex_groups])

    def test_add_attribute_operator(self):
        self.obj.vertex_groups.new(name="UVMap")
        bpy.ops.geometry.attribute_add(name="UVMap", domain='CORNER', data_type='FLOAT2')
        self.assertFalse(self.mesh.attributes.active.name == "UVMap")
        self.assertTrue("UVMap" in [group.name for group in self.obj.vertex_groups])

    def test_add_uv_map(self):
        self.obj.vertex_groups.new(name="UVMap")
        uv_map = self.mesh.uv_layers.new(name="UVMap")
        self.assertFalse(uv_map.name == "UVMap")
        self.assertTrue("UVMap" in [group.name for group in self.obj.vertex_groups])

    def test_convert_to_vertex_group_then_add_uv(self):
        self.mesh.uv_layers.new(name="UVMap")
        self.mesh.attributes.active = self.mesh.attributes["UVMap"]
        bpy.ops.geometry.attribute_convert(mode='VERTEX_GROUP')
        uv_map = self.mesh.uv_layers.new()
        self.assertFalse(uv_map.name == "UVMap")


class TestMeshAttributeConvert(MeshObjectTest):
    def test_convert_active_color_to_generic(self):
        self.mesh.attributes.new("Col", 'FLOAT_COLOR', 'POINT')
        self.mesh.attributes.active = self.mesh.attributes["Col"]
        self.assertTrue(self.mesh.attributes.active_color_name == "Col")
        bpy.ops.geometry.attribute_convert(mode='GENERIC', domain='POINT', data_type='FLOAT')
        self.assertTrue(self.mesh.attributes.active_color_name == "")

    def test_convert_active_color_to_generic_picks_next(self):
        self.mesh.attributes.new("ColA", 'FLOAT_COLOR', 'POINT')
        self.mesh.attributes.new("ColB", 'FLOAT_COLOR', 'POINT')
        self.assertTrue(self.mesh.attributes.active_color_name == "ColA")
        self.mesh.attributes.active = self.mesh.attributes["ColA"]
        bpy.ops.geometry.attribute_convert(mode='GENERIC', domain='POINT', data_type='FLOAT')
        self.assertTrue(self.mesh.attributes.active_color_name == "ColB")

    def test_convert_active_color_to_color_keeps_active(self):
        self.mesh.attributes.new("Col", 'FLOAT_COLOR', 'POINT')
        self.mesh.attributes.active = self.mesh.attributes["Col"]
        bpy.ops.geometry.attribute_convert(mode='GENERIC', domain='POINT', data_type='BYTE_COLOR')
        self.assertTrue(self.mesh.attributes.active_color_name == "Col")

    def test_convert_active_uv_to_uv_keeps_active(self):
        self.mesh.uv_layers.new(name="UVA")
        self.mesh.uv_layers.new(name="UVB")
        self.mesh.uv_layers.active = self.mesh.uv_layers["UVA"]
        self.mesh.attributes.active = self.mesh.attributes["UVA"]
        bpy.ops.geometry.attribute_convert(mode='GENERIC', domain='CORNER', data_type='FLOAT2')
        self.assertTrue(self.mesh.uv_layers.active.name == "UVA")


class TestMeshSkinVertices(MeshObjectTest):
    def test_no_skin_data_by_default(self):
        self.assertEqual(len(self.mesh.skin_vertices), 0)

    def test_add_and_clear_skin_data(self):
        bpy.ops.mesh.customdata_skin_add()
        self.assertEqual(len(self.mesh.skin_vertices), 1)
        skin_layer = self.mesh.skin_vertices[0]
        self.assertEqual(skin_layer.name, "skin_modifier_radius")
        self.assertEqual(len(skin_layer.data), len(self.mesh.vertices))

        bpy.ops.mesh.customdata_skin_clear()
        self.assertEqual(len(self.mesh.skin_vertices), 0)

    def test_skin_vertex_radius(self):
        bpy.ops.mesh.customdata_skin_add()
        skin_layer = self.mesh.skin_vertices[0]

        # Newly added skin data defaults to a radius of 0.25 on both axes.
        vert = skin_layer.data[0]
        self.assertAlmostEqual(vert.radius[0], 0.25)
        self.assertAlmostEqual(vert.radius[1], 0.25)

        vert.radius = (0.5, 0.75)
        self.assertAlmostEqual(skin_layer.data[0].radius[0], 0.5)
        self.assertAlmostEqual(skin_layer.data[0].radius[1], 0.75)

    def test_skin_vert_root_and_loose(self):
        bpy.ops.mesh.customdata_skin_add()
        skin_layer = self.mesh.skin_vertices[0]

        # An arbitrary vertex (the first) is marked as root by default, the rest are not,
        # and no vertex is loose by default.
        self.assertTrue(skin_layer.data[0].use_root)
        self.assertFalse(skin_layer.data[1].use_root)
        self.assertFalse(skin_layer.data[0].use_loose)

        skin_layer.data[1].use_root = True
        self.assertTrue(skin_layer.data[1].use_root)
        skin_layer.data[1].use_root = False
        self.assertFalse(skin_layer.data[1].use_root)

        skin_layer.data[2].use_loose = True
        self.assertTrue(skin_layer.data[2].use_loose)
        skin_layer.data[2].use_loose = False
        self.assertFalse(skin_layer.data[2].use_loose)

    def test_skin_vertices_ignores_unrelated_float2_point_attribute(self):
        # skin_vertices is identified by name, not just by domain and type, so an unrelated
        # float2 point attribute should not be treated as skin data.
        self.mesh.attributes.new("my_custom_data", 'FLOAT2', 'POINT')
        self.assertEqual(len(self.mesh.skin_vertices), 0)

        bpy.ops.mesh.customdata_skin_add()
        self.assertEqual(len(self.mesh.skin_vertices), 1)
        self.assertEqual(self.mesh.skin_vertices[0].name, "skin_modifier_radius")


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
