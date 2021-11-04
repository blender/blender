# Apache License, Version 2.0

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

        assert(len(bpy.data.meshes) == 1)
        assert(len(bpy.data.objects) == 0)
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        output_work_path = os.path.join(output_dir, self.unique_blendfile_name("blendfile"))
        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        # Since there is no usage of linked mesh, it is lost during save/reload.
        assert(len(bpy.data.meshes) == 0)
        assert(orig_data != read_data)

        # Simple link of a single ObData with obdata instanciation.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_object_data=True)

        assert(len(bpy.data.meshes) == 1)
        assert(len(bpy.data.objects) == 1)  # Instance created for the mesh ObData.
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        assert(orig_data == read_data)

        # Simple link of a single Object.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh")

        assert(len(bpy.data.meshes) == 1)
        assert(len(bpy.data.objects) == 1)
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        assert(orig_data == read_data)

        # Simple link of a single Collection, with Empty-instanciation.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Collection")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_collections=True)

        assert(len(bpy.data.meshes) == 1)
        assert(len(bpy.data.objects) == 2)  # linked object and local empty instancing the collection
        assert(len(bpy.data.collections) == 1)  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        assert(orig_data == read_data)

        # Simple link of a single Collection, with ViewLayer-instanciation.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Collection")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh", instance_collections=False)

        assert(len(bpy.data.meshes) == 1)
        assert(len(bpy.data.objects) == 1)
        assert(len(bpy.data.collections) == 1)  # Scene's master collection is not listed here
        # Linked collection should have been added to the scene's master collection children.
        assert(bpy.data.collections[0] in set(bpy.data.scenes[0].collection.children))

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data")

        assert(orig_data == read_data)


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

        print(bpy.data.materials[:], bpy.data.materials[0].library, bpy.data.materials[0].users, bpy.data.materials[0].use_fake_user)

        assert(len(bpy.data.materials) == 1)
        assert(bpy.data.materials[0].library is not None)
        assert(bpy.data.materials[0].users == 2)  # Fake user is not cleared when linking.
        assert(len(bpy.data.meshes) == 1)
        assert(bpy.data.meshes[0].library is None)
        assert(bpy.data.meshes[0].use_fake_user is False)
        assert(bpy.data.meshes[0].users == 0)
        assert(len(bpy.data.objects) == 0)
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here

        # Simple append of a single ObData with obdata instanciation.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=True, set_fake=False, use_recursive=False, do_reuse_local_id=False)

        assert(len(bpy.data.materials) == 1)
        assert(bpy.data.materials[0].library is not None)
        assert(bpy.data.materials[0].users == 2)  # Fake user is not cleared when linking.
        assert(len(bpy.data.meshes) == 1)
        assert(bpy.data.meshes[0].library is None)
        assert(bpy.data.meshes[0].use_fake_user is False)
        assert(bpy.data.meshes[0].users == 1)
        assert(len(bpy.data.objects) == 1)  # Instance created for the mesh ObData.
        assert(bpy.data.objects[0].library is None)
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here

        # Simple append of a single ObData with fake user.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=True, use_recursive=False, do_reuse_local_id=False)

        assert(len(bpy.data.materials) == 1)
        assert(bpy.data.materials[0].library is not None)
        assert(bpy.data.materials[0].users == 2)  # Fake user is not cleared when linking.
        assert(len(bpy.data.meshes) == 1)
        assert(bpy.data.meshes[0].library is None)
        assert(bpy.data.meshes[0].use_fake_user is True)
        assert(bpy.data.meshes[0].users == 1)
        assert(len(bpy.data.objects) == 0)
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here

        # Simple append of a single Object.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=False, do_reuse_local_id=False)

        assert(len(bpy.data.materials) == 1)
        assert(bpy.data.materials[0].library is not None)
        assert(bpy.data.materials[0].users == 2)  # Fake user is not cleared when linking.
        assert(len(bpy.data.meshes) == 1)
        assert(bpy.data.meshes[0].library is None)
        assert(bpy.data.meshes[0].users == 1)
        assert(len(bpy.data.objects) == 1)
        assert(bpy.data.objects[0].library is None)
        assert(bpy.data.objects[0].users == 1)
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here

        # Simple recursive append of a single Object.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Object")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=False)

        assert(len(bpy.data.materials) == 1)
        assert(bpy.data.materials[0].library is None)
        assert(bpy.data.materials[0].users == 1)  # Fake user is cleared when appending.
        assert(len(bpy.data.meshes) == 1)
        assert(bpy.data.meshes[0].library is None)
        assert(bpy.data.meshes[0].users == 1)
        assert(len(bpy.data.objects) == 1)
        assert(bpy.data.objects[0].library is None)
        assert(bpy.data.objects[0].users == 1)
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here

        # Simple recursive append of a single Collection.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Collection")
        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=False)

        assert(len(bpy.data.materials) == 1)
        assert(bpy.data.materials[0].library is None)
        assert(bpy.data.materials[0].users == 1)  # Fake user is cleared when appending.
        assert(bpy.data.meshes[0].library is None)
        assert(bpy.data.meshes[0].users == 1)
        assert(len(bpy.data.objects) == 1)
        assert(bpy.data.objects[0].library is None)
        assert(bpy.data.objects[0].users == 1)
        assert(len(bpy.data.collections) == 1)  # Scene's master collection is not listed here
        assert(bpy.data.collections[0].library is None)
        assert(bpy.data.collections[0].users == 1)


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

        assert(len(bpy.data.meshes) == 1)
        assert(bpy.data.meshes[0].library is None)
        assert(bpy.data.meshes[0].use_fake_user is False)
        assert(bpy.data.meshes[0].users == 1)
        assert(bpy.data.meshes[0].library_weak_reference is not None)
        assert(bpy.data.meshes[0].library_weak_reference.filepath == output_lib_path)
        assert(bpy.data.meshes[0].library_weak_reference.id_name == "MELibMesh")
        assert(len(bpy.data.objects) == 1)
        for ob in bpy.data.objects:
            assert(ob.library is None)
            assert(ob.library_weak_reference is None)
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here

        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=True)

        assert(len(bpy.data.meshes) == 1)
        assert(bpy.data.meshes[0].library is None)
        assert(bpy.data.meshes[0].use_fake_user is False)
        assert(bpy.data.meshes[0].users == 2)
        assert(bpy.data.meshes[0].library_weak_reference is not None)
        assert(bpy.data.meshes[0].library_weak_reference.filepath == output_lib_path)
        assert(bpy.data.meshes[0].library_weak_reference.id_name == "MELibMesh")
        assert(len(bpy.data.objects) == 2)
        for ob in bpy.data.objects:
            assert(ob.library is None)
            assert(ob.library_weak_reference is None)
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here

        bpy.ops.wm.append(directory=link_dir, filename="LibMesh",
                          instance_object_data=False, set_fake=False, use_recursive=True, do_reuse_local_id=False)

        assert(len(bpy.data.meshes) == 2)
        assert(bpy.data.meshes[0].library_weak_reference is None)
        assert(bpy.data.meshes[1].library is None)
        assert(bpy.data.meshes[1].use_fake_user is False)
        assert(bpy.data.meshes[1].users == 1)
        assert(bpy.data.meshes[1].library_weak_reference is not None)
        assert(bpy.data.meshes[1].library_weak_reference.filepath == output_lib_path)
        assert(bpy.data.meshes[1].library_weak_reference.id_name == "MELibMesh")
        assert(len(bpy.data.objects) == 3)
        for ob in bpy.data.objects:
            assert(ob.library is None)
            assert(ob.library_weak_reference is None)
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here


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

        assert(len(bpy.data.meshes) == 1)
        assert(len(bpy.data.objects) == 1)
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        bpy.ops.wm.lib_reload(library=bpy.data.objects[0].name)

        reload_data = self.blender_data_to_tuple(bpy.data, "reload_data")

        print(orig_data)
        print(reload_data)
        assert(orig_data == reload_data)



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

        assert(len(bpy.data.meshes) == 1)
        assert(len(bpy.data.objects) == 1)
        assert(len(bpy.data.collections) == 0)  # Scene's master collection is not listed here

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data")

        lib_path, lib_ext = os.path.splitext(output_lib_path)
        new_lib_path = lib_path + "_relocate" + lib_ext
        os.replace(output_lib_path, new_lib_path)

        bpy.ops.wm.lib_relocate(library=bpy.data.objects[0].name, directory="", filename=new_lib_path)

        relocate_data = self.blender_data_to_tuple(bpy.data, "relocate_data")

        print(orig_data)
        print(relocate_data)
        assert(orig_data == relocate_data)


TESTS = (
    TestBlendLibLinkSaveLoadBasic,
    TestBlendLibAppendBasic,
    TestBlendLibAppendReuseID,
    TestBlendLibLibraryReload,
    TestBlendLibLibraryRelocate,
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
