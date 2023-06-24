# SPDX-FileCopyrightText: 2020-2023 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background -noaudio --python tests/python/bl_blendfile_io.py
import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from bl_blendfile_utils import TestHelper


class TestBlendFileSaveLoadBasic(TestHelper):

    def __init__(self, args):
        self.args = args

    def test_save_load(self):
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)

        bpy.data.meshes.new("OrphanedMesh")

        output_dir = self.args.output_dir
        self.ensure_path(output_dir)

        # Take care to keep the name unique so multiple test jobs can run at once.
        output_path = os.path.join(output_dir, "blendfile_io.blend")

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data 1")

        bpy.ops.wm.save_as_mainfile(filepath=output_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data 1")

        # We have orphaned data, which should be removed by file reading, so there should not be equality here.
        assert orig_data != read_data

        bpy.data.orphans_purge()

        orig_data = self.blender_data_to_tuple(bpy.data, "orig_data 2")

        bpy.ops.wm.save_as_mainfile(filepath=output_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_path, load_ui=False)

        read_data = self.blender_data_to_tuple(bpy.data, "read_data 2")

        assert orig_data == read_data


# NOTE: Technically this should rather be in `bl_id_management.py` test, but that file uses `unittest` module,
#       which makes mixing it with tests system used here and passing extra parameters complicated.
#       Since the main effect of 'RUNTIME' ID tag is on file save, it can as well be here for now.
class TestIdRuntimeTag(TestHelper):

    def __init__(self, args):
        self.args = args

    def unique_blendfile_name(self, base_name):
        return base_name + self.__class__.__name__ + ".blend"

    def test_basics(self):
        output_dir = self.args.output_dir
        self.ensure_path(output_dir)
        bpy.ops.wm.read_homefile(use_empty=False, use_factory_startup=True)

        obj = bpy.data.objects['Cube']
        assert obj.is_runtime_data is False
        assert bpy.context.view_layer.depsgraph.ids['Cube'].is_runtime_data

        output_work_path = os.path.join(output_dir, self.unique_blendfile_name("blendfile"))
        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)

        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)
        obj = bpy.data.objects['Cube']
        assert obj.is_runtime_data is False

        obj.is_runtime_data = True
        assert obj.is_runtime_data

        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)
        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        assert 'Cube' not in bpy.data.objects
        mesh = bpy.data.meshes['Cube']
        assert mesh.is_runtime_data is False
        assert mesh.users == 0

    def test_linking(self):
        output_dir = self.args.output_dir
        self.ensure_path(output_dir)
        bpy.ops.wm.read_homefile(use_empty=False, use_factory_startup=True)

        material = bpy.data.materials.new("LibMaterial")
        # Use a dummy mesh as user of the material, such that the material is saved
        # without having to use fake user on it.
        mesh = bpy.data.meshes.new("LibMesh")
        mesh.materials.append(material)
        mesh.use_fake_user = True

        output_lib_path = os.path.join(output_dir, self.unique_blendfile_name("blendlib_runtimetag_basic"))
        bpy.ops.wm.save_as_mainfile(filepath=output_lib_path, check_existing=False, compress=False)

        bpy.ops.wm.read_homefile(use_empty=False, use_factory_startup=True)

        obj = bpy.data.objects['Cube']
        assert obj.is_runtime_data is False
        obj.is_runtime_data = True

        link_dir = os.path.join(output_lib_path, "Material")
        bpy.ops.wm.link(directory=link_dir, filename="LibMaterial")

        linked_material = bpy.data.materials['LibMaterial']
        assert linked_material.is_library_indirect is False

        link_dir = os.path.join(output_lib_path, "Mesh")
        bpy.ops.wm.link(directory=link_dir, filename="LibMesh")

        linked_mesh = bpy.data.meshes['LibMesh']
        assert linked_mesh.is_library_indirect is False

        obj.data = linked_mesh
        obj.material_slots[0].link = 'OBJECT'
        obj.material_slots[0].material = linked_material

        output_work_path = os.path.join(output_dir, self.unique_blendfile_name("blendfile"))
        bpy.ops.wm.save_as_mainfile(filepath=output_work_path, check_existing=False, compress=False)

        # Only usage of this linked material is a runtime ID (object),
        # so writing .blend file will have properly reset its tag to indirectly linked data.
        assert linked_material.is_library_indirect

        # Only usage of this linked mesh is a runtime ID (object), but it is flagged as 'fake user' in its library,
        # so writing .blend file will have kept its tag to directly linked data.
        assert not linked_mesh.is_library_indirect

        bpy.ops.wm.open_mainfile(filepath=output_work_path, load_ui=False)

        assert 'Cube' not in bpy.data.objects
        assert 'LibMaterial' in bpy.data.materials  # Pulled-in by the linked mesh.
        linked_mesh = bpy.data.meshes['LibMesh']
        assert linked_mesh.use_fake_user is True
        assert linked_mesh.is_library_indirect is False


TESTS = (
    TestBlendFileSaveLoadBasic,

    TestIdRuntimeTag,
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
