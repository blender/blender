# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

"""
./blender.bin --background --python tests/python/bl_blendfile_liblink.py
"""
__all__ = (
    "main",
)

import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from bl_blendfile_utils import TestBlendLibLinkHelper


class TestBlendLibLinkSaveLoadBasic(TestBlendLibLinkHelper):

    def __init__(self, args):
        super().__init__(args)

    def test_link_save_load(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_basic()

        # Simple link of a single ObData.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_object_data=False)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 0)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        output_work_path = os.path.join(output_dir, self.unique_blendfile_name("blendfile"))
        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        # Since there is no usage of linked mesh, it is lost during save/reload.
        self.assertEqual(len(bpy.data.meshes), 0)
        self.assertNotEqual(orig_data, read_data)

        # Simple link of a single ObData with obdata instantiation.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_object_data=True)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)  # Instance created for the mesh ObData.
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        self.assertEqual(orig_data, read_data)

        # Simple link of a single Object.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh")

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        self.assertEqual(orig_data, read_data)

        # Simple link of a single Collection, with Empty-instantiation.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Collection")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_collections=True)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 2)  # linked object and local empty instancing the collection
        self.assertEqual(len(bpy.data.collections), 1)  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        self.assertEqual(orig_data, read_data)

        # Simple link of a single Collection, with ViewLayer-instantiation.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Collection")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_collections=False)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)
        self.assertEqual(len(bpy.data.collections), 1)  # Scene's master collection is not listed here
        # Linked collection should have been added to the scene's master collection children.
        self.assertIn(bpy.data.collections[0], set(bpy.data.scenes[0].collection.children))

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        self.assertEqual(orig_data, read_data)


class TestBlendLibLinkIndirect(TestBlendLibLinkHelper):

    def __init__(self, args):
        super().__init__(args)

    def test_append(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_indirect_lib()

        # Simple link of a single ObData.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_object_data=False)

        self.assertEqual(len(bpy.data.images), 1)
        self.assertEqual(len(bpy.data.materials), 1)
        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 0)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        mesh = bpy.data.meshes[0]
        material = bpy.data.materials[0]
        image = bpy.data.images[0]

        self.assertIsNotNone(image.library)
        self.assertFalse(image.use_fake_user)  # Fake user is cleared when linking.
        self.assertEqual(image.users, 1)
        self.assertTrue(image.is_library_indirect)
        self.assertNotEqual(len(image.pixels), 0)
        self.assertTrue(image.has_data)

        self.assertIsNotNone(material.library)
        self.assertFalse(material.use_fake_user)  # Fake user is cleared when linking.
        self.assertEqual(material.users, 1)
        self.assertTrue(material.is_library_indirect)

        self.assertIsNotNone(mesh.library)
        self.assertFalse(mesh.use_fake_user)
        self.assertEqual(mesh.users, 0)
        # IDs explicitly linked by the user are forcefully considered directly linked.
        self.assertFalse(mesh.is_library_indirect)

        ob = bpy.data.objects.new("LocalMesh", mesh)
        coll = bpy.data.collections.new("LocalMesh")
        coll.objects.link(ob)
        bpy.context.scene.collection.children.link(coll)

        self.assertEqual(image.users, 1)
        self.assertTrue(image.is_library_indirect)
        self.assertEqual(material.users, 1)
        self.assertTrue(material.is_library_indirect)
        self.assertEqual(mesh.users, 1)
        self.assertFalse(mesh.is_library_indirect)

        ob.material_slots[0].link = 'OBJECT'
        ob.material_slots[0].material = material

        self.assertEqual(image.users, 1)
        self.assertTrue(image.is_library_indirect)
        self.assertEqual(material.users, 2)
        self.assertFalse(material.is_library_indirect)

        ob.material_slots[0].material = None

        self.assertEqual(image.users, 1)
        self.assertTrue(image.is_library_indirect)
        self.assertEqual(material.users, 1)
        # This is not properly updated whene removing a local user of linked data.
        self.assertFalse(material.is_library_indirect)

        output_work_path = os.path.join(output_dir, self.unique_blendfile_name("blendfile"))
        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)

        self.assertEqual(image.users, 1)
        self.assertTrue(image.is_library_indirect)
        self.assertEqual(material.users, 1)
        self.assertTrue(material.is_library_indirect)

        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        self.assertEqual(len(bpy.data.images), 1)
        self.assertEqual(len(bpy.data.materials), 1)
        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)
        self.assertEqual(len(bpy.data.collections), 1)  # Scene's master collection is not listed here

        mesh = bpy.data.meshes[0]
        material = bpy.data.materials[0]
        image = bpy.data.images[0]

        self.assertIsNotNone(image.library)
        self.assertFalse(image.use_fake_user)  # Fake user is cleared when linking.
        self.assertEqual(image.users, 1)
        self.assertTrue(image.is_library_indirect)

        self.assertIsNotNone(material.library)
        self.assertFalse(material.use_fake_user)  # Fake user is cleared when linking.
        self.assertEqual(material.users, 1)
        self.assertTrue(material.is_library_indirect)

        self.assertIsNotNone(mesh.library)
        self.assertFalse(mesh.use_fake_user)
        self.assertEqual(mesh.users, 1)
        self.assertFalse(mesh.is_library_indirect)


class TestBlendLibLinkAnimation(TestBlendLibLinkHelper):

    def __init__(self, args):
        super().__init__(args)

    def test_link(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_animated()

        # Simple link of a the collection, and check animation values.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Collection")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_collections=False, instance_object_data=False)

        self.assertIsNotNone(bpy.data.meshes[0].library)
        self.assertEqual(bpy.data.meshes[0].users, 1)
        self.assertEqual(len(bpy.data.objects), 2)
        self.assertIsNotNone(bpy.data.objects[0].library)
        self.assertEqual(bpy.data.objects[0].users, 1)
        self.assertIsNotNone(bpy.data.objects[1].library)
        self.assertEqual(bpy.data.objects[1].users, 1)
        self.assertEqual(len(bpy.data.collections), 1)  # Scene's master collection is not listed here
        self.assertIsNotNone(bpy.data.collections[0].library)
        self.assertEqual(bpy.data.collections[0].users, 1)
        self.assertEqual(len(bpy.data.actions), 2)
        self.assertIsNotNone(bpy.data.actions[0].library)
        self.assertEqual(bpy.data.actions[0].users, 1)
        self.assertIsNotNone(bpy.data.actions[1].library)
        self.assertEqual(bpy.data.actions[1].users, 1)

        # Validate animation evaluation.
        bpy.context.scene.frame_set(1)
        self.assertEqual(bpy.data.objects["LibController"].location[0], 0.0)
        self.assertEqual(bpy.data.objects["LibMesh"].location[0], bpy.data.objects["LibController"].location[0])
        self.assertEqual(bpy.data.objects["LibMesh"].location[1], 0.0)
        bpy.context.scene.frame_set(10)
        self.assertEqual(bpy.data.objects["LibController"].location[0], 5.0)
        self.assertEqual(bpy.data.objects["LibMesh"].location[0], bpy.data.objects["LibController"].location[0])
        self.assertEqual(bpy.data.objects["LibMesh"].location[1], -5.0)


class TestBlendLibAppendBasic(TestBlendLibLinkHelper):

    def __init__(self, args):
        super().__init__(args)

    def test_append(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_indirect_lib()

        # Simple append of a single ObData.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=False, do_reuse_local_id=False)

        self.assertEqual(len(bpy.data.images), 1)
        self.assertIsNotNone(bpy.data.images[0].library)
        self.assertEqual(bpy.data.images[0].users, 1)
        self.assertNotEqual(len(bpy.data.images[0].pixels), 0)
        self.assertTrue(bpy.data.images[0].has_data)
        self.assertEqual(len(bpy.data.materials), 1)
        self.assertIsNotNone(bpy.data.materials[0].library)
        self.assertEqual(bpy.data.materials[0].users, 1)  # Fake user is cleared when linking.
        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertIsNone(bpy.data.meshes[0].library)
        self.assertFalse(bpy.data.meshes[0].use_fake_user)
        self.assertEqual(bpy.data.meshes[0].users, 0)
        self.assertEqual(len(bpy.data.objects), 0)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        # Simple append of a single ObData with obdata instantiation.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=True, set_fake=False, use_recursive=False, do_reuse_local_id=False)

        self.assertEqual(len(bpy.data.images), 1)
        self.assertIsNotNone(bpy.data.images[0].library)
        self.assertEqual(bpy.data.images[0].users, 1)
        self.assertNotEqual(len(bpy.data.images[0].pixels), 0)
        self.assertTrue(bpy.data.images[0].has_data)
        self.assertEqual(len(bpy.data.materials), 1)
        self.assertIsNotNone(bpy.data.materials[0].library)
        self.assertEqual(bpy.data.materials[0].users, 1)  # Fake user is cleared when linking.
        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertIsNone(bpy.data.meshes[0].library)
        self.assertFalse(bpy.data.meshes[0].use_fake_user)
        self.assertEqual(bpy.data.meshes[0].users, 1)
        self.assertEqual(len(bpy.data.objects), 1)  # Instance created for the mesh ObData.
        self.assertIsNone(bpy.data.objects[0].library)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        # Simple append of a single ObData with fake user.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=True, use_recursive=False, do_reuse_local_id=False)

        self.assertEqual(len(bpy.data.images), 1)
        self.assertIsNotNone(bpy.data.images[0].library)
        self.assertEqual(bpy.data.images[0].users, 1)
        self.assertNotEqual(len(bpy.data.images[0].pixels), 0)
        self.assertTrue(bpy.data.images[0].has_data)
        self.assertEqual(len(bpy.data.materials), 1)
        self.assertIsNotNone(bpy.data.materials[0].library)
        self.assertEqual(bpy.data.materials[0].users, 1)  # Fake user is cleared when linking.
        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertIsNone(bpy.data.meshes[0].library)
        self.assertTrue(bpy.data.meshes[0].use_fake_user)
        self.assertEqual(bpy.data.meshes[0].users, 1)
        self.assertEqual(len(bpy.data.objects), 0)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        # Simple append of a single Object.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=False, do_reuse_local_id=False)

        self.assertEqual(len(bpy.data.images), 1)
        self.assertIsNotNone(bpy.data.images[0].library)
        self.assertEqual(bpy.data.images[0].users, 1)
        self.assertNotEqual(len(bpy.data.images[0].pixels), 0)
        self.assertTrue(bpy.data.images[0].has_data)
        self.assertEqual(len(bpy.data.materials), 1)
        self.assertIsNotNone(bpy.data.materials[0].library)
        self.assertEqual(bpy.data.materials[0].users, 1)  # Fake user is cleared when linking.
        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertIsNone(bpy.data.meshes[0].library)
        self.assertEqual(bpy.data.meshes[0].users, 1)
        self.assertEqual(len(bpy.data.objects), 1)
        self.assertIsNone(bpy.data.objects[0].library)
        self.assertEqual(bpy.data.objects[0].users, 1)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        # Simple recursive append of a single Object.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=False)

        self.assertEqual(len(bpy.data.images), 1)
        self.assertIsNone(bpy.data.images[0].library)
        self.assertEqual(bpy.data.images[0].users, 1)
        self.assertNotEqual(len(bpy.data.images[0].pixels), 0)
        self.assertTrue(bpy.data.images[0].has_data)
        self.assertEqual(len(bpy.data.materials), 1)
        self.assertIsNone(bpy.data.materials[0].library)
        self.assertEqual(bpy.data.materials[0].users, 1)  # Fake user is cleared when appending.
        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertIsNone(bpy.data.meshes[0].library)
        self.assertEqual(bpy.data.meshes[0].users, 1)
        self.assertEqual(len(bpy.data.objects), 1)
        self.assertIsNone(bpy.data.objects[0].library)
        self.assertEqual(bpy.data.objects[0].users, 1)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        # Simple recursive append of a single Collection.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Collection")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=False)

        self.assertEqual(len(bpy.data.images), 1)
        self.assertIsNone(bpy.data.images[0].library)
        self.assertEqual(bpy.data.images[0].users, 1)
        self.assertNotEqual(len(bpy.data.images[0].pixels), 0)
        self.assertTrue(bpy.data.images[0].has_data)
        self.assertEqual(len(bpy.data.materials), 1)
        self.assertIsNone(bpy.data.materials[0].library)
        self.assertEqual(bpy.data.materials[0].users, 1)  # Fake user is cleared when appending.
        self.assertIsNone(bpy.data.meshes[0].library)
        self.assertEqual(bpy.data.meshes[0].users, 1)
        self.assertEqual(len(bpy.data.objects), 1)
        self.assertIsNone(bpy.data.objects[0].library)
        self.assertEqual(bpy.data.objects[0].users, 1)
        self.assertEqual(len(bpy.data.collections), 1)  # Scene's master collection is not listed here
        self.assertIsNone(bpy.data.collections[0].library)
        self.assertEqual(bpy.data.collections[0].users, 1)


class TestBlendLibAppendReuseID(TestBlendLibLinkHelper):

    def __init__(self, args):
        super().__init__(args)

    def test_append(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_basic()

        # Append of a single Object, and then append it again.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=False)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertIsNone(bpy.data.meshes[0].library)
        self.assertFalse(bpy.data.meshes[0].use_fake_user)
        self.assertEqual(bpy.data.meshes[0].users, 1)
        self.assertIsNotNone(bpy.data.meshes[0].library_weak_reference)
        self.assertEqual(bpy.data.meshes[0].library_weak_reference.filepath, output_lib_path)
        self.assertEqual(bpy.data.meshes[0].library_weak_reference.id_name, "MELibMesh")
        self.assertEqual(len(bpy.data.objects), 1)
        for ob in bpy.data.objects:
            self.assertIsNone(ob.library)
            self.assertIsNone(ob.library_weak_reference)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=True)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertIsNone(bpy.data.meshes[0].library)
        self.assertFalse(bpy.data.meshes[0].use_fake_user)
        self.assertEqual(bpy.data.meshes[0].users, 2)
        self.assertIsNotNone(bpy.data.meshes[0].library_weak_reference)
        self.assertEqual(bpy.data.meshes[0].library_weak_reference.filepath, output_lib_path)
        self.assertEqual(bpy.data.meshes[0].library_weak_reference.id_name, "MELibMesh")
        self.assertEqual(len(bpy.data.objects), 2)
        for ob in bpy.data.objects:
            self.assertIsNone(ob.library)
            self.assertIsNone(ob.library_weak_reference)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=False)

        self.assertEqual(len(bpy.data.meshes), 2)
        self.assertIsNone(bpy.data.meshes[0].library_weak_reference)
        self.assertIsNone(bpy.data.meshes[1].library)
        self.assertFalse(bpy.data.meshes[1].use_fake_user)
        self.assertEqual(bpy.data.meshes[1].users, 1)
        self.assertIsNotNone(bpy.data.meshes[1].library_weak_reference)
        self.assertEqual(bpy.data.meshes[1].library_weak_reference.filepath, output_lib_path)
        self.assertEqual(bpy.data.meshes[1].library_weak_reference.id_name, "MELibMesh")
        self.assertEqual(len(bpy.data.objects), 3)
        for ob in bpy.data.objects:
            self.assertIsNone(ob.library)
            self.assertIsNone(ob.library_weak_reference)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here


class TestBlendLibPackedLinkedID(TestBlendLibLinkHelper):

    def __init__(self, args):
        super().__init__(args)

    def test_link_pack_basic(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_basic()

        # Link of a single Object, and make it packed.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_object_data=False)

        self.assertEqual(len(bpy.data.libraries), 1)
        library = bpy.data.libraries[0]

        self.assertEqual(len(bpy.data.meshes), 1)
        for me in bpy.data.meshes:
            self.assertEqual(me.library, library)
            self.assertEqual(me.users, 1)
        self.assertEqual(len(bpy.data.objects), 1)
        for ob in bpy.data.objects:
            self.assertEqual(ob.library, library)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        ob_packed = bpy.data.pack_linked_ids_hierarchy(bpy.data.objects[0])

        # Need to ensure that the newly packed linked object is used, and kept in the scene.
        bpy.data.scenes[0].collection.objects.link(ob_packed)

        self.assertEqual(len(bpy.data.libraries), 2)
        library = bpy.data.libraries[0]
        archive_library = bpy.data.libraries[1]

        def check_valid():
            self.assertFalse(library.is_archive)
            self.assertEqual(len(library.archive_libraries), 1)
            self.assertEqual(library.archive_libraries[0], archive_library)
            self.assertTrue(archive_library.is_archive)
            self.assertEqual(archive_library.archive_parent_library, library)

            self.assertEqual(len(bpy.data.meshes), 2)
            self.assertEqual(bpy.data.meshes[0].library, library)
            self.assertEqual(bpy.data.meshes[0].users, 1)
            self.assertEqual(bpy.data.meshes[1].library, archive_library)
            self.assertEqual(bpy.data.meshes[1].users, 1)

            self.assertEqual(len(bpy.data.objects), 2)
            self.assertEqual(bpy.data.objects[0].library, library)
            self.assertEqual(bpy.data.objects[0].data, bpy.data.meshes[0])
            self.assertEqual(bpy.data.objects[1].library, archive_library)
            self.assertEqual(bpy.data.objects[1].data, bpy.data.meshes[1])

        check_valid()

        output_work_path = os.path.join(output_dir, self.unique_blendfile_name("blendfile"))
        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)

        self.reset_blender()

        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        self.assertEqual(len(bpy.data.libraries), 2)
        library = bpy.data.libraries[0]
        archive_library = bpy.data.libraries[1]

        check_valid()

    def test_link_pack_indirect(self):
        # Test handling of indirectly linked packed data (when packed in another library),
        # packing linked data using other packed linked data, etc.
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_packed_indirect_lib()

        # Link of a single Object, and make it packed.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_object_data=False)

        # Directly linked library, indirectly linked one (though empty), and its packed archive version.
        self.assertEqual(len(bpy.data.libraries), 3)
        library = bpy.data.libraries[0]
        library_indirect = bpy.data.libraries[1]
        library_indirect_archive = bpy.data.libraries[2]

        def check_valid():
            self.assertFalse(library.is_archive)
            self.assertFalse(library_indirect.is_archive)
            self.assertTrue(library_indirect_archive.is_archive)
            self.assertTrue(library_indirect_archive.name in library_indirect.archive_libraries)

            self.assertEqual(len(bpy.data.images), 1)
            for im in bpy.data.images:
                self.assertEqual(im.library, library_indirect_archive)
                self.assertEqual(im.users, 1)
                self.assertTrue(im.is_linked_packed)

            self.assertEqual(len(bpy.data.materials), 1)
            for ma in bpy.data.materials:
                self.assertEqual(ma.library, library_indirect_archive)
                self.assertEqual(ma.users, 1)
                self.assertTrue(ma.is_linked_packed)

            self.assertEqual(len(bpy.data.meshes), 1)
            for me in bpy.data.meshes:
                self.assertEqual(me.library, library)
                self.assertEqual(me.users, 1)

            self.assertEqual(len(bpy.data.objects), 1)
            for ob in bpy.data.objects:
                self.assertEqual(ob.library, library)

            self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        check_valid()

        output_work_path = os.path.join(output_dir, self.unique_blendfile_name("blendfile"))
        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)

        self.reset_blender()

        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        self.assertEqual(len(bpy.data.libraries), 3)
        library = bpy.data.libraries[0]
        library_indirect = bpy.data.libraries[1]
        library_indirect_archive = bpy.data.libraries[2]

        check_valid()

        ob_packed = bpy.data.pack_linked_ids_hierarchy(bpy.data.objects[0])

        # Need to ensure that the newly packed linked object is used, and kept in the scene.
        bpy.data.scenes[0].collection.objects.link(ob_packed)

        self.assertEqual(len(bpy.data.libraries), 4)
        # Due to ID name sorting, the newly ceratedt archive library should be second now, after its parent one.
        archive_library = bpy.data.libraries[1]

        def check_valid():
            self.assertFalse(library.is_archive)
            self.assertEqual(len(library.archive_libraries), 1)
            self.assertEqual(library.archive_libraries[0], archive_library)
            self.assertTrue(archive_library.is_archive)
            self.assertEqual(archive_library.archive_parent_library, library)

            self.assertFalse(library_indirect.is_archive)
            self.assertTrue(library_indirect_archive.is_archive)
            self.assertTrue(library_indirect_archive.name in library_indirect.archive_libraries)

            self.assertEqual(len(bpy.data.images), 1)
            for im in bpy.data.images:
                self.assertEqual(im.library, library_indirect_archive)
                self.assertEqual(im.users, 1)
                self.assertTrue(im.is_linked_packed)

            self.assertEqual(len(bpy.data.materials), 1)
            for ma in bpy.data.materials:
                self.assertEqual(ma.library, library_indirect_archive)
                self.assertEqual(ma.users, 2)
                self.assertTrue(ma.is_linked_packed)

            self.assertEqual(len(bpy.data.meshes), 2)
            self.assertEqual(bpy.data.meshes[0].library, library)
            self.assertEqual(bpy.data.meshes[0].users, 1)
            self.assertEqual(bpy.data.meshes[1].library, archive_library)
            self.assertEqual(bpy.data.meshes[1].users, 1)

            self.assertEqual(len(bpy.data.objects), 2)
            self.assertEqual(bpy.data.objects[0].library, library)
            self.assertEqual(bpy.data.objects[0].data, bpy.data.meshes[0])
            self.assertEqual(bpy.data.objects[1].library, archive_library)
            self.assertEqual(bpy.data.objects[1].data, bpy.data.meshes[1])

            self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        check_valid()

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)

        self.reset_blender()

        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        self.assertEqual(len(bpy.data.libraries), 4)
        library = bpy.data.libraries[0]
        archive_library = bpy.data.libraries[1]
        library_indirect = bpy.data.libraries[2]
        library_indirect_archive = bpy.data.libraries[3]

        check_valid()


class TestBlendLibLibraryReload(TestBlendLibLinkHelper):

    def __init__(self, args):
        super().__init__(args)

    def test_link_reload(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_basic()

        # Simple link of a single Object, and reload.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh")

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.lib_reload(library=bpy.data.objects[0].name)

        reload_data = self.blender_data_to_tuple(bpy.data, "reload_data")

        self.assertEqual(orig_data, reload_data)


class TestBlendLibLibraryRelocate(TestBlendLibLinkHelper):

    def __init__(self, args):
        super().__init__(args)

    def test_link_relocate(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_basic()

        # Simple link of a single Object, and reload.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh")

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)
        self.assertEqual(len(bpy.data.collections), 0)  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        lib_path, lib_ext = os.path.splitext(output_lib_path)
        new_lib_path = lib_path + "_relocate" + lib_ext
        os.replace(output_lib_path, new_lib_path)

        bpy.ops.wm.lib_relocate(library=bpy.data.objects[0].name, directory="", filename=new_lib_path)

        relocate_data = self.blender_data_to_tuple(bpy.data, "relocate_data")

        self.assertEqual(orig_data, relocate_data)


# Python library loader context manager.
class TestBlendLibDataLibrariesLoad(TestBlendLibLinkHelper):

    def __init__(self, args):
        super().__init__(args)

    def do_libload_init(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_basic()

        # Simple link of a single Object, and reload.
        self.reset_blender()

        return output_lib_path

    def do_libload(self, **load_kwargs):
        with bpy.data.libraries.load(**load_kwargs) as lib_ctx:
            lib_src, lib_link = lib_ctx

            self.assertEqual(len(lib_src.meshes), 1)
            self.assertEqual(len(lib_src.objects), 1)
            self.assertEqual(len(lib_src.collections), 1)

            self.assertEqual(len(lib_link.meshes), 0)
            self.assertEqual(len(lib_link.objects), 0)
            self.assertEqual(len(lib_link.collections), 0)

            lib_link.collections.append(lib_src.collections[0])

        # Linking/append/liboverride happens when living the context manager.


class TestBlendLibDataLibrariesLoadAppend(TestBlendLibDataLibrariesLoad):

    def test_libload_append(self):
        output_lib_path = self.do_libload_init()
        self.do_libload(filepath=output_lib_path, link=False, create_liboverrides=False)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)  # This code does no instantiation.
        self.assertEqual(len(bpy.data.collections), 1)

        # Append, so all data should have been made local.
        self.assertIsNone(bpy.data.meshes[0].library)
        self.assertIsNone(bpy.data.objects[0].library)
        self.assertIsNone(bpy.data.collections[0].library)


class TestBlendLibDataLibrariesLoadLink(TestBlendLibDataLibrariesLoad):

    def test_libload_link(self):
        output_lib_path = self.do_libload_init()
        self.do_libload(filepath=output_lib_path, link=True, create_liboverrides=False)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)  # This code does no instantiation.
        self.assertEqual(len(bpy.data.collections), 1)

        # Link, so all data should have remained linked.
        self.assertIsNotNone(bpy.data.meshes[0].library)
        self.assertIsNotNone(bpy.data.objects[0].library)
        self.assertIsNotNone(bpy.data.collections[0].library)


class TestBlendLibDataLibrariesLoadPack(TestBlendLibDataLibrariesLoad):

    def test_libload_pack(self):
        output_lib_path = self.do_libload_init()
        # Cannot create overrides on packed linked data currently.
        self.assertRaises(ValueError,
                          self.do_libload, filepath=output_lib_path, link=True, pack=True, create_liboverrides=True)
        self.do_libload(filepath=output_lib_path, link=True, pack=True, create_liboverrides=False)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)  # This code does no instantiation.
        self.assertEqual(len(bpy.data.collections), 1)
        # One archive library for the packed data-blocks and the reference library.
        self.assertEqual(len(bpy.data.libraries), 2)

        # Packed dat should be owned by archive library.
        packed_mesh = bpy.data.meshes[0]
        packed_object = bpy.data.objects[0]
        packed_collection = bpy.data.collections[0]
        link_library = bpy.data.libraries[0]
        archive_library = bpy.data.libraries[1]
        self.assertEqual(packed_mesh.library, archive_library)
        self.assertEqual(packed_object.library, archive_library)
        self.assertEqual(packed_collection.library, archive_library)
        self.assertTrue(archive_library.is_archive)
        self.assertFalse(link_library.is_archive)


class TestBlendLibDataLibrariesLoadLibOverride(TestBlendLibDataLibrariesLoad):

    def test_libload_liboverride(self):
        output_lib_path = self.do_libload_init()
        self.do_libload(filepath=output_lib_path, link=True, create_liboverrides=True)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)  # This code does no instantiation.
        self.assertEqual(len(bpy.data.collections), 2)  # The linked one and its local liboverride.

        # Link + LibOverride, so linked data should have remained linked.
        self.assertIsNotNone(bpy.data.meshes[-1].library)
        self.assertIsNotNone(bpy.data.objects[-1].library)
        self.assertIsNotNone(bpy.data.collections[-1].library)

        # Only explicitly linked data gets a liboverride, without any handling of hierarchy/dependencies.
        self.assertIsNone(bpy.data.collections[0].library)
        self.assertFalse(bpy.data.collections[0].is_runtime_data)
        self.assertIsNotNone(bpy.data.collections[0].override_library)
        self.assertEqual(bpy.data.collections[0].override_library.reference, bpy.data.collections[-1])

        # Should create another liboverride for the linked collection.
        self.do_libload(filepath=output_lib_path, link=True, create_liboverrides=True, reuse_liboverrides=False)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)  # This code does no instantiation.
        self.assertEqual(len(bpy.data.collections), 3)  # The linked one and its two local liboverrides.

        # Link + LibOverride, so linked data should have remained linked.
        self.assertIsNotNone(bpy.data.meshes[-1].library)
        self.assertIsNotNone(bpy.data.objects[-1].library)
        self.assertIsNotNone(bpy.data.collections[-1].library)

        # Only explicitly linked data gets a liboverride, without any handling of hierarchy/dependencies.
        self.assertIsNone(bpy.data.collections[1].library)
        self.assertFalse(bpy.data.collections[1].is_runtime_data)
        self.assertIsNotNone(bpy.data.collections[1].override_library)
        self.assertEqual(bpy.data.collections[1].override_library.reference, bpy.data.collections[-1])

        # This call should not change anything, first liboverrides should be found and 'reused'.
        self.do_libload(filepath=output_lib_path, link=True, create_liboverrides=True, reuse_liboverrides=True)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)  # This code does no instantiation.
        self.assertEqual(len(bpy.data.collections), 3)  # The linked one and its two local liboverrides.

        # Link + LibOverride, so linked data should have remained linked.
        self.assertIsNotNone(bpy.data.meshes[-1].library)
        self.assertIsNotNone(bpy.data.objects[-1].library)
        self.assertIsNotNone(bpy.data.collections[-1].library)

        # Only explicitly linked data gets a liboverride, without any handling of hierarchy/dependencies.
        self.assertIsNone(bpy.data.collections[1].library)
        self.assertFalse(bpy.data.collections[1].is_runtime_data)
        self.assertIsNotNone(bpy.data.collections[1].override_library)
        self.assertEqual(bpy.data.collections[1].override_library.reference, bpy.data.collections[-1])

    def test_libload_liboverride_runtime(self):
        output_lib_path = self.do_libload_init()
        self.do_libload(filepath=output_lib_path, link=True,
                        create_liboverrides=True,
                        create_liboverrides_runtime=True)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)  # This code does no instantiation.
        self.assertEqual(len(bpy.data.collections), 2)  # The linked one and its local liboverride.

        # Link + LibOverride, so linked data should have remained linked.
        self.assertIsNotNone(bpy.data.meshes[-1].library)
        self.assertIsNotNone(bpy.data.objects[-1].library)
        self.assertIsNotNone(bpy.data.collections[-1].library)

        # Only explicitly linked data gets a liboverride, without any handling of hierarchy/dependencies.
        self.assertIsNone(bpy.data.collections[0].library)
        self.assertTrue(bpy.data.collections[0].is_runtime_data)
        self.assertIsNotNone(bpy.data.collections[0].override_library)
        self.assertEqual(bpy.data.collections[0].override_library.reference, bpy.data.collections[-1])

        # This call should not change anything, first liboverrides should be found and 'reused'.
        self.do_libload(filepath=output_lib_path,
                        link=True,
                        create_liboverrides=True,
                        create_liboverrides_runtime=True,
                        reuse_liboverrides=True)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)  # This code does no instantiation.
        self.assertEqual(len(bpy.data.collections), 2)  # The linked one and its local liboverride.

        # Link + LibOverride, so linked data should have remained linked.
        self.assertIsNotNone(bpy.data.meshes[-1].library)
        self.assertIsNotNone(bpy.data.objects[-1].library)
        self.assertIsNotNone(bpy.data.collections[-1].library)

        # Only explicitly linked data gets a liboverride, without any handling of hierarchy/dependencies.
        self.assertIsNone(bpy.data.collections[0].library)
        self.assertTrue(bpy.data.collections[0].is_runtime_data)
        self.assertIsNotNone(bpy.data.collections[0].override_library)
        self.assertEqual(bpy.data.collections[0].override_library.reference, bpy.data.collections[-1])

        # Should create another liboverride for the linked collection, since this time we request a non-runtime one.
        self.do_libload(filepath=output_lib_path,
                        link=True,
                        create_liboverrides=True,
                        create_liboverrides_runtime=False,
                        reuse_liboverrides=True)

        self.assertEqual(len(bpy.data.meshes), 1)
        self.assertEqual(len(bpy.data.objects), 1)  # This code does no instantiation.
        self.assertEqual(len(bpy.data.collections), 3)  # The linked one and its two local liboverrides.

        # Link + LibOverride, so linked data should have remained linked.
        self.assertIsNotNone(bpy.data.meshes[-1].library)
        self.assertIsNotNone(bpy.data.objects[-1].library)
        self.assertIsNotNone(bpy.data.collections[-1].library)

        # Only explicitly linked data gets a liboverride, without any handling of hierarchy/dependencies.
        self.assertIsNone(bpy.data.collections[1].library)
        self.assertFalse(bpy.data.collections[1].is_runtime_data)
        self.assertIsNotNone(bpy.data.collections[1].override_library)
        self.assertEqual(bpy.data.collections[1].override_library.reference, bpy.data.collections[-1])


TESTS = (
    TestBlendLibLinkSaveLoadBasic,
    TestBlendLibLinkAnimation,
    TestBlendLibLinkIndirect,

    TestBlendLibAppendBasic,
    TestBlendLibAppendReuseID,

    TestBlendLibPackedLinkedID,

    TestBlendLibLibraryReload,
    TestBlendLibLibraryRelocate,

    TestBlendLibDataLibrariesLoadAppend,
    TestBlendLibDataLibrariesLoadLink,
    TestBlendLibDataLibrariesLoadPack,
    TestBlendLibDataLibrariesLoadLibOverride,
)


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    description = "Test basic IO of blend file."
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "--src-test-dir",
        dest="src_test_dir",
        default=".",
        help="Where to find test/data root directory",
        required=True,
    )
    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        default=".",
        help="Where to output temp saved blendfiles",
        required=False,
    )

    return parser


def main():
    args = argparse_create().parse_args()

    # Don't write thumbnails into the home directory.
    bpy.context.preferences.filepaths.file_preview_type = 'NONE'

    for Test in TESTS:
        Test(args).run_all_tests()


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    main()
