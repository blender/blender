# Apache License, Version 2.0

# ./blender.bin --background -noaudio --python tests/python/bl_blendfile_library_overrides.py -- --output-dir=/tmp/
import pathlib
import bpy
import sys
import os
import unittest

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from bl_blendfile_utils import TestHelper


class TestLibraryOverrides(TestHelper, unittest.TestCase):
    MESH_LIBRARY_PARENT = "LibMeshParent"
    OBJECT_LIBRARY_PARENT = "LibMeshParent"
    MESH_LIBRARY_CHILD = "LibMeshChild"
    OBJECT_LIBRARY_CHILD = "LibMeshChild"
    MESH_LIBRARY_PERMISSIVE = "LibMeshPermissive"
    OBJECT_LIBRARY_PERMISSIVE = "LibMeshPermissive"

    def __init__(self, args):
        self.args = args

        output_dir = pathlib.Path(self.args.output_dir)
        self.ensure_path(str(output_dir))
        self.output_path = output_dir / "blendlib_overrides.blend"
        self.test_output_path = output_dir / "blendlib_overrides_test.blend"

        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        mesh = bpy.data.meshes.new(TestLibraryOverrides.MESH_LIBRARY_PARENT)
        obj = bpy.data.objects.new(TestLibraryOverrides.OBJECT_LIBRARY_PARENT, object_data=mesh)
        bpy.context.collection.objects.link(obj)
        mesh_child = bpy.data.meshes.new(TestLibraryOverrides.MESH_LIBRARY_CHILD)
        obj_child = bpy.data.objects.new(TestLibraryOverrides.OBJECT_LIBRARY_CHILD, object_data=mesh_child)
        obj_child.parent = obj
        bpy.context.collection.objects.link(obj_child)

        mesh = bpy.data.meshes.new(TestLibraryOverrides.MESH_LIBRARY_PERMISSIVE)
        obj = bpy.data.objects.new(TestLibraryOverrides.OBJECT_LIBRARY_PERMISSIVE, object_data=mesh)
        bpy.context.collection.objects.link(obj)
        obj.override_template_create()
        prop = obj.override_library.properties.add(rna_path='scale')
        prop.operations.add(operation='NOOP')

        bpy.ops.wm.save_as_mainfile(filepath=str(self.output_path), check_existing=False, compress=False)

    def __ensure_override_library_updated(self):
        # During save the override_library is updated.
        bpy.ops.wm.save_as_mainfile(filepath=str(self.test_output_path), check_existing=False, compress=False)

    def test_link_and_override_property(self):
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        bpy.data.orphans_purge()

        link_dir = self.output_path / "Object"
        bpy.ops.wm.link(directory=str(link_dir), filename=TestLibraryOverrides.OBJECT_LIBRARY_PARENT)

        obj = bpy.data.objects[TestLibraryOverrides.OBJECT_LIBRARY_PARENT]
        self.assertIsNone(obj.override_library)
        local_id = obj.override_create()
        self.assertIsNotNone(local_id.override_library)
        self.assertIsNone(local_id.data.override_library)
        assert(len(local_id.override_library.properties) == 0)

        local_id.location.y = 1.0

        self.__ensure_override_library_updated()

        assert(len(local_id.override_library.properties) == 1)
        override_prop = local_id.override_library.properties[0]
        assert(override_prop.rna_path == "location");
        assert(len(override_prop.operations) == 1)
        override_operation = override_prop.operations[0]
        assert(override_operation.operation == 'REPLACE')
        # Setting location.y overridded all elements in the location array. -1 is a wildcard.
        assert(override_operation.subitem_local_index == -1)

    def test_link_permissive(self):
        """
        Linked assets with a permissive template.

        - Checks if the NOOP is properly handled.
        - Checks if the correct properties and operations are created/updated.
        """
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        bpy.data.orphans_purge()

        link_dir = self.output_path / "Object"
        bpy.ops.wm.link(directory=str(link_dir), filename=TestLibraryOverrides.OBJECT_LIBRARY_PERMISSIVE)

        obj = bpy.data.objects[TestLibraryOverrides.OBJECT_LIBRARY_PERMISSIVE]
        self.assertIsNotNone(obj.override_library)
        local_id = obj.override_create()
        self.assertIsNotNone(local_id.override_library)
        self.assertIsNone(local_id.data.override_library)
        assert(len(local_id.override_library.properties) == 1)
        override_prop = local_id.override_library.properties[0]
        assert(override_prop.rna_path == "scale");
        assert(len(override_prop.operations) == 1)
        override_operation = override_prop.operations[0]
        assert(override_operation.operation == 'NOOP')
        assert(override_operation.subitem_local_index == -1)

        local_id.location.y = 1.0
        local_id.scale.x = 0.5
        # `scale.x` will apply, but will be reverted when the library overrides
        # are updated. This is by design so python scripts can still alter the
        # properties locally what is a typical usecase in productions.
        assert(local_id.scale.x == 0.5)
        assert(local_id.location.y == 1.0)

        self.__ensure_override_library_updated()
        assert(local_id.scale.x == 1.0)
        assert(local_id.location.y == 1.0)

        assert(len(local_id.override_library.properties) == 2)
        override_prop = local_id.override_library.properties[0]
        assert(override_prop.rna_path == "scale");
        assert(len(override_prop.operations) == 1)
        override_operation = override_prop.operations[0]
        assert(override_operation.operation == 'NOOP')
        assert(override_operation.subitem_local_index == -1)

        override_prop = local_id.override_library.properties[1]
        assert(override_prop.rna_path == "location");
        assert(len(override_prop.operations) == 1)
        override_operation = override_prop.operations[0]
        assert(override_operation.operation == 'REPLACE')
        assert (override_operation.subitem_local_index == -1)


class TestLibraryTemplate(TestHelper, unittest.TestCase):
    MESH_LIBRARY_PERMISSIVE = "LibMeshPermissive"
    OBJECT_LIBRARY_PERMISSIVE = "LibMeshPermissive"

    def __init__(self, args):
        pass

    def test_permissive_template(self):
        """
        Test setting up a permissive template.
        """
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        mesh = bpy.data.meshes.new(TestLibraryTemplate.MESH_LIBRARY_PERMISSIVE)
        obj = bpy.data.objects.new(TestLibraryTemplate.OBJECT_LIBRARY_PERMISSIVE, object_data=mesh)
        bpy.context.collection.objects.link(obj)
        assert(obj.override_library is None)
        obj.override_template_create()
        assert(obj.override_library is not None)
        assert(len(obj.override_library.properties) == 0)
        prop = obj.override_library.properties.add(rna_path='scale')
        assert(len(obj.override_library.properties) == 1)
        assert(len(prop.operations) == 0)
        operation = prop.operations.add(operation='NOOP')
        assert(len(prop.operations) == 1)
        assert(operation.operation == 'NOOP')


TESTS = (
    TestLibraryOverrides,
    TestLibraryTemplate,
)


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    description = "Test library overrides of blend file."
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
    bpy.context.preferences.filepaths.use_save_preview_images = False
    bpy.context.preferences.experimental.use_override_templates = True

    for Test in TESTS:
        Test(args).run_all_tests()


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + \
        (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    main()
