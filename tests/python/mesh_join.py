# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


import unittest
import bpy
import sys
import pathlib
from mathutils import Vector


class TestMeshJoin(unittest.TestCase):

    def test_simple(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()
        bpy.ops.mesh.primitive_cube_add()
        bpy.ops.mesh.primitive_cube_add()
        mesh = bpy.data.objects['Cube'].data
        bpy.ops.object.select_all(action='SELECT')
        bpy.context.view_layer.objects.active = bpy.data.objects['Cube']
        bpy.ops.object.join()
        self.assertEqual(len(mesh.vertices), 16)

    def test_face_sets(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()
        bpy.ops.mesh.primitive_cube_add()
        bpy.ops.mesh.primitive_monkey_add()
        bpy.ops.mesh.primitive_uv_sphere_add()
        cube_obj = bpy.data.objects['Cube']
        cube = cube_obj.data
        monkey = bpy.data.objects['Suzanne'].data
        sphere = bpy.data.objects['Sphere'].data
        attr = cube.attributes.new(name=".sculpt_face_set", type='INT', domain='FACE')
        attr.data[0].value = 1
        attr.data[1].value = 1
        attr.data[2].value = 45
        attr.data[3].value = 45
        attr.data[4].value = 45
        attr.data[5].value = 45
        b = monkey.attributes.new(name=".sculpt_face_set", type='INT', domain='FACE')
        b.data[0].value = 52
        b.data[1].value = 52
        b.data[2].value = 52
        b.data[3].value = 52
        b.data[4].value = 52
        b.data[5].value = 52
        sphere.attributes.new(name=".sculpt_face_set", type='INT', domain='FACE')

        bpy.ops.object.select_all(action='SELECT')
        bpy.context.view_layer.objects.active = cube_obj
        bpy.ops.object.join()

        joined_attr = cube.attributes[".sculpt_face_set"]
        self.assertEqual(len(joined_attr.data), 1018)
        self.assertEqual(joined_attr.data[0].value, 1)
        self.assertEqual(joined_attr.data[1].value, 1)
        self.assertEqual(joined_attr.data[2].value, 45)
        self.assertEqual(joined_attr.data[9].value, 98)
        self.assertEqual(joined_attr.data[20].value, 46)

    def test_materials_simple(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()
        mat_1 = bpy.data.materials.new('mat_1')

        bpy.ops.mesh.primitive_cube_add()
        cube_1 = bpy.context.view_layer.objects.active
        cube_1.data.materials.append(mat_1)

        bpy.ops.mesh.primitive_cube_add()
        cube_2 = bpy.context.view_layer.objects.active
        cube_2.data.materials.append(mat_1)

        bpy.ops.object.select_all(action='SELECT')
        bpy.context.view_layer.objects.active = cube_1
        bpy.ops.object.join()

        result_mesh = cube_1.data
        self.assertEqual(len(result_mesh.materials), 1)
        self.assertFalse(result_mesh.attributes.get("material_index"))

    def test_no_materials_with_indices(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()
        mat_1 = bpy.data.materials.new('mat_1')

        bpy.ops.mesh.primitive_cube_add()
        cube_1 = bpy.context.view_layer.objects.active
        cube_1.data.materials.append(mat_1)
        material_indices = cube_1.data.attributes.new(name="material_index", type='INT', domain='FACE')

        bpy.ops.mesh.primitive_cube_add()
        cube_2 = bpy.context.view_layer.objects.active
        material_indices = cube_2.data.attributes.new(name="material_index", type='INT', domain='FACE')
        material_indices.data.foreach_set('value', [0, 1, 1, 700, 1, 2])

        bpy.ops.object.select_all(action='SELECT')
        bpy.context.view_layer.objects.active = cube_1
        bpy.ops.object.join()

        result_mesh = cube_1.data
        self.assertEqual(len(result_mesh.materials), 2)
        material_indices = cube_1.data.attributes["material_index"]
        self.assertTrue(material_indices)
        material_index_data = [m.value for m in material_indices.data]
        self.assertEqual(material_index_data, [0] * 6 + [0, 1, 1, 700, 1, 2])

    def test_materials(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()
        mat_1 = bpy.data.materials.new('mat_1')
        mat_2 = bpy.data.materials.new('mat_2')
        mat_3 = bpy.data.materials.new('mat_3')
        mat_4 = bpy.data.materials.new('mat_4')

        bpy.ops.mesh.primitive_cube_add()
        cube_no_mats = bpy.context.view_layer.objects.active

        bpy.ops.mesh.primitive_cube_add()
        cube_2 = bpy.context.view_layer.objects.active
        cube_2.data.materials.append(mat_1)
        cube_2.data.materials.append(mat_2)
        cube_2.data.materials.append(mat_3)
        material_indices = cube_2.data.attributes.new(name="material_index", type='INT', domain='FACE')
        material_indices.data.foreach_set('value', [0, 1, 2, 0, 1, 2])

        bpy.ops.mesh.primitive_cube_add()
        cube_2_mats = bpy.context.view_layer.objects.active
        cube_2_mats.data.materials.append(mat_3)
        cube_2_mats.data.materials.append(mat_4)
        material_indices = cube_2_mats.data.attributes.new(name="material_index", type='INT', domain='FACE')
        material_indices.data.foreach_set('value', [0, 0, 0, 1, 1, 1])

        bpy.ops.object.select_all(action='SELECT')
        bpy.context.view_layer.objects.active = cube_no_mats
        bpy.ops.object.join()

        result_mesh = cube_no_mats.data
        self.assertEqual(len(result_mesh.materials), 5)
        material_indices = result_mesh.attributes["material_index"]
        material_index_data = [m.value for m in material_indices.data]
        self.assertEqual(material_index_data, [0] * 6 + [1, 2, 3, 1, 2, 3] + [3, 3, 3, 4, 4, 4])

    def test_shared_object_data(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()

        bpy.ops.mesh.primitive_cube_add()
        cube_1 = bpy.context.view_layer.objects.active

        bpy.ops.object.duplicate(linked=True)
        cube_2 = bpy.context.view_layer.objects.active
        self.assertEqual(cube_1.data.name, cube_2.data.name)

        bpy.ops.mesh.primitive_cube_add()

        bpy.ops.object.select_all(action='SELECT')
        bpy.context.view_layer.objects.active = cube_1
        bpy.ops.object.join()

        self.assertEqual(len(cube_1.data.vertices), 24)

    def test_shape_keys(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()

        bpy.ops.mesh.primitive_cube_add()
        cube_1 = bpy.context.view_layer.objects.active
        cube_1.shape_key_add(name="Basis")
        key = cube_1.shape_key_add(name="A")
        key = cube_1.shape_key_add(name="B")
        cube_1.data.shape_keys.key_blocks["B"].data[0].co = Vector((-1, -4, -1))

        bpy.ops.mesh.primitive_cube_add()
        cube_2 = bpy.context.view_layer.objects.active
        cube_2.shape_key_add(name="Basis")
        key = cube_2.shape_key_add(name="A")
        key = cube_2.shape_key_add(name="B")
        key = cube_2.shape_key_add(name="C")
        key = cube_2.shape_key_add(name="D")
        cube_2.data.shape_keys.key_blocks["B"].data[5].co = Vector((1, -3, 1))

        bpy.ops.object.select_all(action='SELECT')
        bpy.context.view_layer.objects.active = cube_1
        bpy.ops.object.join()

        self.assertEqual(len(cube_1.data.vertices), 16)
        self.assertEqual(len(cube_1.data.shape_keys.key_blocks), 5)
        self.assertEqual(cube_1.data.shape_keys.key_blocks["B"].data[0].co, Vector((-1.0, -4.0, -1.0)))
        self.assertEqual(cube_1.data.shape_keys.key_blocks["B"].data[13].co, Vector((1.0, -3.0, 1.0)))

    def test_shape_keys_not_active(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()

        bpy.ops.mesh.primitive_cube_add()
        cube_1 = bpy.context.object
        cube_1.shape_key_add(name="Basis")
        key = cube_1.shape_key_add(name="A")
        key = cube_1.shape_key_add(name="B")
        key = cube_1.shape_key_add(name="C")

        bpy.ops.mesh.primitive_cube_add()
        cube_2 = bpy.context.object

        bpy.ops.object.select_all(action='SELECT')
        bpy.context.view_layer.objects.active = cube_2
        bpy.ops.object.join()

        self.assertEqual(len(cube_2.data.vertices), 16)
        self.assertEqual(len(cube_2.data.shape_keys.key_blocks), 4)

    def test_no_faces(self):
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()

        bpy.ops.mesh.primitive_cube_add()
        bpy.ops.mesh.primitive_cube_add()
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.mode_set(mode='EDIT', toggle=False)
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.delete(type='ONLY_FACE')
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        bpy.ops.object.join()

        joined = bpy.context.object
        self.assertEqual(len(joined.data.vertices), 16)
        self.assertEqual(len(joined.data.polygons), 0)


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
