# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background -noaudio --python tests/python/bl_blendfile_liblink.py
import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from bl_blendfile_utils import TestHelper


class TestBlendLibLinkHelper(TestHelper):

    def __init__(self, args):
        self.args = args

    @staticmethod
    def reset_blender():
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        bpy.data.orphans_purge(do_recursive=True)

    def unique_blendfile_name(self, base_name):
        return base_name + self.__class__.__name__ + ".blend"

    def init_lib_data_basic(self):
        self.reset_blender()

        me = bpy.data.meshes.new("LibMesh")
        ob = bpy.data.objects.new("LibMesh", me)
        coll = bpy.data.collections.new("LibMesh")
        coll.objects.link(ob)
        bpy.context.scene.collection.children.link(coll)

        output_dir = self.args.output_dir
        self.ensure_path(output_dir)
        # Take care to keep the name unique so multiple test jobs can run at once.
        output_lib_path = os.path.join(output_dir, self.unique_blendfile_name("blendlib_basic"))

        bpy.ops.wm.save_as_mainfile(filepath=output_lib_path, check_existing=False, compress=False)

        return output_lib_path

    def init_lib_data_animated(self):
        self.reset_blender()

        me = bpy.data.meshes.new("LibMesh")
        ob = bpy.data.objects.new("LibMesh", me)
        ob_ctrl = bpy.data.objects.new("LibController", None)
        coll = bpy.data.collections.new("LibMesh")
        coll.objects.link(ob)
        coll.objects.link(ob_ctrl)
        bpy.context.scene.collection.children.link(coll)

        # Add some action & driver animation to `LibMesh`.
        # Animate Y location.
        ob.location[1] = 0.0
        ob.keyframe_insert("location", index=1, frame=1)
        ob.location[1] = -5.0
        ob.keyframe_insert("location", index=1, frame=10)

        # Drive X location.
        ob_drv = ob.driver_add("location", 0)
        ob_drv.driver.type = 'AVERAGE'
        ob_drv_var = ob_drv.driver.variables.new()
        ob_drv_var.type = 'TRANSFORMS'
        ob_drv_var.targets[0].id = ob_ctrl
        ob_drv_var.targets[0].transform_type = 'LOC_X'

        # Add some action & driver animation to `LibController`.
        # Animate X location.
        ob_ctrl.location[0] = 0.0
        ob_ctrl.keyframe_insert("location", index=0, frame=1)
        ob_ctrl.location[0] = 5.0
        ob_ctrl.keyframe_insert("location", index=0, frame=10)

        output_dir = self.args.output_dir
        self.ensure_path(output_dir)
        # Take care to keep the name unique so multiple test jobs can run at once.
        output_lib_path = os.path.join(output_dir, self.unique_blendfile_name("blendlib_animated"))

        bpy.ops.wm.save_as_mainfile(filepath=output_lib_path, check_existing=False, compress=False)

        return output_lib_path

    def init_lib_data_indirect_lib(self):
        output_dir = self.args.output_dir
        self.ensure_path(output_dir)

        # Create an indirect library containing a material.
        self.reset_blender()

        ma = bpy.data.materials.new("LibMaterial")
        ma.use_fake_user = True
        # Take care to keep the name unique so multiple test jobs can run at once.
        output_lib_path = os.path.join(output_dir, self.unique_blendfile_name("blendlib_indirect_material"))

        bpy.ops.wm.save_as_mainfile(filepath=output_lib_path, check_existing=False, compress=False)

        # Create a main library containing object etc., and linking material from indirect library.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Material")
        bpy.ops.wm.link(directory=link_dir, filename="LibMaterial")
        ma = bpy.data.materials[0]

        me = bpy.data.meshes.new("LibMesh")
        me.materials.append(ma)
        ob = bpy.data.objects.new("LibMesh", me)
        coll = bpy.data.collections.new("LibMesh")
        coll.objects.link(ob)
        bpy.context.scene.collection.children.link(coll)

        output_dir = self.args.output_dir
        self.ensure_path(output_dir)
        # Take care to keep the name unique so multiple test jobs can run at once.
        output_lib_path = os.path.join(output_dir, self.unique_blendfile_name("blendlib_indirect_main"))

        bpy.ops.wm.save_as_mainfile(filepath=output_lib_path, check_existing=False, compress=False)

        return output_lib_path


class TestBlendLibLinkSaveLoadBasic(TestBlendLibLinkHelper):

    def __init__(self, args):
        self.args = args

    def test_link_save_load(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_basic()

        # Simple link of a single ObData.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_object_data=False)

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 0
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        output_work_path = os.path.join(output_dir, self.unique_blendfile_name("blendfile"))
        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        # Since there is no usage of linked mesh, it is lost during save/reload.
        assert len(bpy.data.meshes) == 0
        assert orig_data != read_data

        # Simple link of a single ObData with obdata instantiation.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_object_data=True)

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1  # Instance created for the mesh ObData.
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        assert orig_data == read_data

        # Simple link of a single Object.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh")

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        assert orig_data == read_data

        # Simple link of a single Collection, with Empty-instantiation.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Collection")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_collections=True)

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 2  # linked object and local empty instancing the collection
        assert len(bpy.data.collections) == 1  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        assert orig_data == read_data

        # Simple link of a single Collection, with ViewLayer-instantiation.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Collection")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_collections=False)

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1
        assert len(bpy.data.collections) == 1  # Scene's master collection is not listed here
        # Linked collection should have been added to the scene's master collection children.
        assert bpy.data.collections[0] in set(bpy.data.scenes[0].collection.children)

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        assert orig_data == read_data


class TestBlendLibLinkIndirect(TestBlendLibLinkHelper):

    def __init__(self, args):
        self.args = args

    def test_append(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_indirect_lib()

        # Simple link of a single ObData.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_object_data=False)

        assert len(bpy.data.materials) == 1
        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 0
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        mesh = bpy.data.meshes[0]
        material = bpy.data.materials[0]

        assert material.library is not None
        assert material.use_fake_user is False  # Fake user is cleared when linking.
        assert material.users == 1
        assert material.is_library_indirect is True

        assert mesh.library is not None
        assert mesh.use_fake_user is False
        assert mesh.users == 0
        # IDs explicitely linked by the user are forcefully considered directly linked.
        assert mesh.is_library_indirect is False

        ob = bpy.data.objects.new("LocalMesh", mesh)
        coll = bpy.data.collections.new("LocalMesh")
        coll.objects.link(ob)
        bpy.context.scene.collection.children.link(coll)

        assert material.users == 1
        assert material.is_library_indirect is True
        assert mesh.users == 1
        assert mesh.is_library_indirect is False

        ob.material_slots[0].link = 'OBJECT'
        ob.material_slots[0].material = material

        assert material.users == 2
        assert material.is_library_indirect is False

        ob.material_slots[0].material = None

        assert material.users == 1
        # This is not properly updated whene removing a local user of linked data.
        assert material.is_library_indirect is False

        output_work_path = os.path.join(output_dir, self.unique_blendfile_name("blendfile"))
        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)

        assert material.users == 1
        assert material.is_library_indirect is True

        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        assert len(bpy.data.materials) == 1
        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1
        assert len(bpy.data.collections) == 1  # Scene's master collection is not listed here

        mesh = bpy.data.meshes[0]
        material = bpy.data.materials[0]

        assert material.library is not None
        assert material.use_fake_user is False  # Fake user is cleared when linking.
        assert material.users == 1
        assert material.is_library_indirect is True

        assert mesh.library is not None
        assert mesh.use_fake_user is False
        assert mesh.users == 1
        assert mesh.is_library_indirect is False


class TestBlendLibLinkAnimation(TestBlendLibLinkHelper):

    def __init__(self, args):
        self.args = args

    def test_link(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_animated()

        # Simple link of a the collection, and check animation values.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Collection")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_collections=False, instance_object_data=False)

        assert bpy.data.meshes[0].library
        assert bpy.data.meshes[0].users == 1
        assert len(bpy.data.objects) == 2
        assert bpy.data.objects[0].library
        assert bpy.data.objects[0].users == 1
        assert bpy.data.objects[1].library
        assert bpy.data.objects[1].users == 1
        assert len(bpy.data.collections) == 1  # Scene's master collection is not listed here
        assert bpy.data.collections[0].library
        assert bpy.data.collections[0].users == 1
        assert len(bpy.data.actions) == 2
        assert bpy.data.actions[0].library
        assert bpy.data.actions[0].users == 1
        assert bpy.data.actions[1].library
        assert bpy.data.actions[1].users == 1

        # Validate animation evaluation.
        bpy.context.scene.frame_set(10)
        print(bpy.data.objects["LibController"].location)
        print(bpy.data.objects["LibMesh"].location)
        bpy.context.scene.frame_set(1)
        print(bpy.data.objects["LibController"].location)
        print(bpy.data.objects["LibMesh"].location)
        assert bpy.data.objects["LibController"].location[0] == 0.0
        assert bpy.data.objects["LibMesh"].location[0] == bpy.data.objects["LibController"].location[0]
        assert bpy.data.objects["LibMesh"].location[1] == 0.0
        bpy.context.scene.frame_set(10)
        assert bpy.data.objects["LibController"].location[0] == 5.0
        assert bpy.data.objects["LibMesh"].location[0] == bpy.data.objects["LibController"].location[0]
        assert bpy.data.objects["LibMesh"].location[1] == -5.0



class TestBlendLibAppendBasic(TestBlendLibLinkHelper):

    def __init__(self, args):
        self.args = args

    def test_append(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_indirect_lib()

        # Simple append of a single ObData.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=False, do_reuse_local_id=False)

        assert len(bpy.data.materials) == 1
        assert bpy.data.materials[0].library is not None
        assert bpy.data.materials[0].users == 1  # Fake user is cleared when linking.
        assert len(bpy.data.meshes) == 1
        assert bpy.data.meshes[0].library is None
        assert bpy.data.meshes[0].use_fake_user is False
        assert bpy.data.meshes[0].users == 0
        assert len(bpy.data.objects) == 0
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        # Simple append of a single ObData with obdata instantiation.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=True, set_fake=False, use_recursive=False, do_reuse_local_id=False)

        assert len(bpy.data.materials) == 1
        assert bpy.data.materials[0].library is not None
        assert bpy.data.materials[0].users == 1  # Fake user is cleared when linking.
        assert len(bpy.data.meshes) == 1
        assert bpy.data.meshes[0].library is None
        assert bpy.data.meshes[0].use_fake_user is False
        assert bpy.data.meshes[0].users == 1
        assert len(bpy.data.objects) == 1  # Instance created for the mesh ObData.
        assert bpy.data.objects[0].library is None
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        # Simple append of a single ObData with fake user.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=True, use_recursive=False, do_reuse_local_id=False)

        assert len(bpy.data.materials) == 1
        assert bpy.data.materials[0].library is not None
        assert bpy.data.materials[0].users == 1  # Fake user is cleared when linking.
        assert len(bpy.data.meshes) == 1
        assert bpy.data.meshes[0].library is None
        assert bpy.data.meshes[0].use_fake_user is True
        assert bpy.data.meshes[0].users == 1
        assert len(bpy.data.objects) == 0
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        # Simple append of a single Object.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=False, do_reuse_local_id=False)

        assert len(bpy.data.materials) == 1
        assert bpy.data.materials[0].library is not None
        assert bpy.data.materials[0].users == 1  # Fake user is cleared when linking.
        assert len(bpy.data.meshes) == 1
        assert bpy.data.meshes[0].library is None
        assert bpy.data.meshes[0].users == 1
        assert len(bpy.data.objects) == 1
        assert bpy.data.objects[0].library is None
        assert bpy.data.objects[0].users == 1
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        # Simple recursive append of a single Object.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=False)

        assert len(bpy.data.materials) == 1
        assert bpy.data.materials[0].library is None
        assert bpy.data.materials[0].users == 1  # Fake user is cleared when appending.
        assert len(bpy.data.meshes) == 1
        assert bpy.data.meshes[0].library is None
        assert bpy.data.meshes[0].users == 1
        assert len(bpy.data.objects) == 1
        assert bpy.data.objects[0].library is None
        assert bpy.data.objects[0].users == 1
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        # Simple recursive append of a single Collection.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Collection")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=False)

        assert len(bpy.data.materials) == 1
        assert bpy.data.materials[0].library is None
        assert bpy.data.materials[0].users == 1  # Fake user is cleared when appending.
        assert bpy.data.meshes[0].library is None
        assert bpy.data.meshes[0].users == 1
        assert len(bpy.data.objects) == 1
        assert bpy.data.objects[0].library is None
        assert bpy.data.objects[0].users == 1
        assert len(bpy.data.collections) == 1  # Scene's master collection is not listed here
        assert bpy.data.collections[0].library is None
        assert bpy.data.collections[0].users == 1


class TestBlendLibAppendReuseID(TestBlendLibLinkHelper):

    def __init__(self, args):
        self.args = args

    def test_append(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_basic()

        # Append of a single Object, and then append it again.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=False)

        assert len(bpy.data.meshes) == 1
        assert bpy.data.meshes[0].library is None
        assert bpy.data.meshes[0].use_fake_user is False
        assert bpy.data.meshes[0].users == 1
        assert bpy.data.meshes[0].library_weak_reference is not None
        assert bpy.data.meshes[0].library_weak_reference.filepath == output_lib_path
        assert bpy.data.meshes[0].library_weak_reference.id_name == "MELibMesh"
        assert len(bpy.data.objects) == 1
        for ob in bpy.data.objects:
            assert ob.library is None
            assert ob.library_weak_reference is None
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=True)

        assert len(bpy.data.meshes) == 1
        assert bpy.data.meshes[0].library is None
        assert bpy.data.meshes[0].use_fake_user is False
        assert bpy.data.meshes[0].users == 2
        assert bpy.data.meshes[0].library_weak_reference is not None
        assert bpy.data.meshes[0].library_weak_reference.filepath == output_lib_path
        assert bpy.data.meshes[0].library_weak_reference.id_name == "MELibMesh"
        assert len(bpy.data.objects) == 2
        for ob in bpy.data.objects:
            assert ob.library is None
            assert ob.library_weak_reference is None
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=False)

        assert len(bpy.data.meshes) == 2
        assert bpy.data.meshes[0].library_weak_reference is None
        assert bpy.data.meshes[1].library is None
        assert bpy.data.meshes[1].use_fake_user is False
        assert bpy.data.meshes[1].users == 1
        assert bpy.data.meshes[1].library_weak_reference is not None
        assert bpy.data.meshes[1].library_weak_reference.filepath == output_lib_path
        assert bpy.data.meshes[1].library_weak_reference.id_name == "MELibMesh"
        assert len(bpy.data.objects) == 3
        for ob in bpy.data.objects:
            assert ob.library is None
            assert ob.library_weak_reference is None
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here


class TestBlendLibLibraryReload(TestBlendLibLinkHelper):

    def __init__(self, args):
        self.args = args

    def test_link_reload(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_basic()

        # Simple link of a single Object, and reload.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh")

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.lib_reload(library=bpy.data.objects[0].name)

        reload_data = self.blender_data_to_tuple(bpy.data, "reload_data")

        print(orig_data)
        print(reload_data)
        assert orig_data == reload_data


class TestBlendLibLibraryRelocate(TestBlendLibLinkHelper):

    def __init__(self, args):
        self.args = args

    def test_link_relocate(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_basic()

        # Simple link of a single Object, and reload.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh")

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1
        assert len(bpy.data.collections) == 0  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        lib_path, lib_ext = os.path.splitext(output_lib_path)
        new_lib_path = lib_path + "_relocate" + lib_ext
        os.replace(output_lib_path, new_lib_path)

        bpy.ops.wm.lib_relocate(library=bpy.data.objects[0].name, directory="", filename=new_lib_path)

        relocate_data = self.blender_data_to_tuple(bpy.data, "relocate_data")

        print(orig_data)
        print(relocate_data)
        assert orig_data == relocate_data


# Python library loader context manager.
class TestBlendLibDataLibrariesLoad(TestBlendLibLinkHelper):

    def __init__(self, args):
        self.args = args

    def do_libload_init(self):
        output_dir = self.args.output_dir
        output_lib_path = self.init_lib_data_basic()

        # Simple link of a single Object, and reload.
        self.reset_blender()

        return output_lib_path

    def do_libload(self, **load_kwargs):
        with bpy.data.libraries.load(**load_kwargs) as lib_ctx:
            lib_src, lib_link = lib_ctx

            assert len(lib_src.meshes) == 1
            assert len(lib_src.objects) == 1
            assert len(lib_src.collections) == 1

            assert len(lib_link.meshes) == 0
            assert len(lib_link.objects) == 0
            assert len(lib_link.collections) == 0

            lib_link.collections.append(lib_src.collections[0])

        # Linking/append/liboverride happens when living the context manager.


class TestBlendLibDataLibrariesLoadAppend(TestBlendLibDataLibrariesLoad):

    def test_libload_append(self):
        output_lib_path = self.do_libload_init()
        self.do_libload(filepath=output_lib_path, link=False, create_liboverrides=False)

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1  # This code does no instantiation.
        assert len(bpy.data.collections) == 1

        # Append, so all data should have been made local.
        assert bpy.data.meshes[0].library is None
        assert bpy.data.objects[0].library is None
        assert bpy.data.collections[0].library is None


class TestBlendLibDataLibrariesLoadLink(TestBlendLibDataLibrariesLoad):

    def test_libload_link(self):
        output_lib_path = self.do_libload_init()
        self.do_libload(filepath=output_lib_path, link=True, create_liboverrides=False)

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1  # This code does no instantiation.
        assert len(bpy.data.collections) == 1

        # Link, so all data should have remained linked.
        assert bpy.data.meshes[0].library is not None
        assert bpy.data.objects[0].library is not None
        assert bpy.data.collections[0].library is not None


class TestBlendLibDataLibrariesLoadLibOverride(TestBlendLibDataLibrariesLoad):

    def test_libload_liboverride(self):
        output_lib_path = self.do_libload_init()
        self.do_libload(filepath=output_lib_path, link=True, create_liboverrides=True)

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1  # This code does no instantiation.
        assert len(bpy.data.collections) == 2  # The linked one and its local liboverride.

        # Link + LibOverride, so linked data should have remained linked.
        assert bpy.data.meshes[-1].library is not None
        assert bpy.data.objects[-1].library is not None
        assert bpy.data.collections[-1].library is not None

        # Only explicitely linked data gets a liboverride, without any handling of hierarchy/dependencies.
        assert bpy.data.collections[0].library is None
        assert bpy.data.collections[0].is_runtime_data is False
        assert bpy.data.collections[0].override_library is not None
        assert bpy.data.collections[0].override_library.reference == bpy.data.collections[-1]

        # Should create another liboverride for the linked collection.
        self.do_libload(filepath=output_lib_path, link=True, create_liboverrides=True, reuse_liboverrides=False)

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1  # This code does no instantiation.
        assert len(bpy.data.collections) == 3  # The linked one and its two local liboverrides.

        # Link + LibOverride, so linked data should have remained linked.
        assert bpy.data.meshes[-1].library is not None
        assert bpy.data.objects[-1].library is not None
        assert bpy.data.collections[-1].library is not None

        # Only explicitely linked data gets a liboverride, without any handling of hierarchy/dependencies.
        assert bpy.data.collections[1].library is None
        assert bpy.data.collections[1].is_runtime_data is False
        assert bpy.data.collections[1].override_library is not None
        assert bpy.data.collections[1].override_library.reference == bpy.data.collections[-1]

        # This call should not change anything, first liboverrides should be found and 'reused'.
        self.do_libload(filepath=output_lib_path, link=True, create_liboverrides=True, reuse_liboverrides=True)

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1  # This code does no instantiation.
        assert len(bpy.data.collections) == 3  # The linked one and its two local liboverrides.

        # Link + LibOverride, so linked data should have remained linked.
        assert bpy.data.meshes[-1].library is not None
        assert bpy.data.objects[-1].library is not None
        assert bpy.data.collections[-1].library is not None

        # Only explicitely linked data gets a liboverride, without any handling of hierarchy/dependencies.
        assert bpy.data.collections[1].library is None
        assert bpy.data.collections[1].is_runtime_data is False
        assert bpy.data.collections[1].override_library is not None
        assert bpy.data.collections[1].override_library.reference == bpy.data.collections[-1]

    def test_libload_liboverride_runtime(self):
        output_lib_path = self.do_libload_init()
        self.do_libload(filepath=output_lib_path, link=True,
                        create_liboverrides=True,
                        create_liboverrides_runtime=True)

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1  # This code does no instantiation.
        assert len(bpy.data.collections) == 2  # The linked one and its local liboverride.

        # Link + LibOverride, so linked data should have remained linked.
        assert bpy.data.meshes[-1].library is not None
        assert bpy.data.objects[-1].library is not None
        assert bpy.data.collections[-1].library is not None

        # Only explicitely linked data gets a liboverride, without any handling of hierarchy/dependencies.
        assert bpy.data.collections[0].library is None
        assert bpy.data.collections[0].is_runtime_data is True
        assert bpy.data.collections[0].override_library is not None
        assert bpy.data.collections[0].override_library.reference == bpy.data.collections[-1]

        # This call should not change anything, first liboverrides should be found and 'reused'.
        self.do_libload(filepath=output_lib_path,
                        link=True,
                        create_liboverrides=True,
                        create_liboverrides_runtime=True,
                        reuse_liboverrides=True)

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1  # This code does no instantiation.
        assert len(bpy.data.collections) == 2  # The linked one and its local liboverride.

        # Link + LibOverride, so linked data should have remained linked.
        assert bpy.data.meshes[-1].library is not None
        assert bpy.data.objects[-1].library is not None
        assert bpy.data.collections[-1].library is not None

        # Only explicitely linked data gets a liboverride, without any handling of hierarchy/dependencies.
        assert bpy.data.collections[0].library is None
        assert bpy.data.collections[0].is_runtime_data is True
        assert bpy.data.collections[0].override_library is not None
        assert bpy.data.collections[0].override_library.reference == bpy.data.collections[-1]

        # Should create another liboverride for the linked collection, since this time we request a non-runtime one.
        self.do_libload(filepath=output_lib_path,
                        link=True,
                        create_liboverrides=True,
                        create_liboverrides_runtime=False,
                        reuse_liboverrides=True)

        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.objects) == 1  # This code does no instantiation.
        assert len(bpy.data.collections) == 3  # The linked one and its two local liboverrides.

        # Link + LibOverride, so linked data should have remained linked.
        assert bpy.data.meshes[-1].library is not None
        assert bpy.data.objects[-1].library is not None
        assert bpy.data.collections[-1].library is not None

        # Only explicitely linked data gets a liboverride, without any handling of hierarchy/dependencies.
        assert bpy.data.collections[1].library is None
        assert bpy.data.collections[1].is_runtime_data is False
        assert bpy.data.collections[1].override_library is not None
        assert bpy.data.collections[1].override_library.reference == bpy.data.collections[-1]


TESTS = (
    TestBlendLibLinkSaveLoadBasic,
    TestBlendLibLinkAnimation,
    TestBlendLibLinkIndirect,

    TestBlendLibAppendBasic,
    TestBlendLibAppendReuseID,

    TestBlendLibLibraryReload,
    TestBlendLibLibraryRelocate,

    TestBlendLibDataLibrariesLoadAppend,
    TestBlendLibDataLibrariesLoadLink,
    TestBlendLibDataLibrariesLoadLibOverride,
)


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    description = "Test basic IO of blend file."
    parser = argparse.ArgumentParser(description=description)
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
