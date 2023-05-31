# SPDX-License-Identifier: Apache-2.0

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
        assert len(local_id.override_library.properties) == 0

        # #### Generate an override property & operation automatically by editing the local override data.
        local_id.location.y = 1.0
        local_id.override_library.operations_update()
        assert len(local_id.override_library.properties) == 1
        override_prop = local_id.override_library.properties[0]
        assert override_prop.rna_path == "location"
        assert len(override_prop.operations) == 1
        override_operation = override_prop.operations[0]
        assert override_operation.operation == 'REPLACE'
        # Setting location.y overrode all elements in the location array. -1 is a wildcard.
        assert override_operation.subitem_local_index == -1

        # #### Reset the override to its linked reference data.
        local_id.override_library.reset()
        assert len(local_id.override_library.properties) == 0
        assert local_id.location == local_id.override_library.reference.location

        # #### Generate an override property & operation manually using the API.
        override_property = local_id.override_library.properties.add(rna_path="location")
        override_property.operations.add(operation='REPLACE')

        assert len(local_id.override_library.properties) == 1
        override_prop = local_id.override_library.properties[0]
        assert override_prop.rna_path == "location"
        assert len(override_prop.operations) == 1
        override_operation = override_prop.operations[0]
        assert override_operation.operation == 'REPLACE'
        # Setting location.y overrode all elements in the location array. -1 is a wildcard.
        assert override_operation.subitem_local_index == -1

        override_property = local_id.override_library.properties[0]
        override_property.operations.remove(override_property.operations[0])
        local_id.override_library.properties.remove(override_property)

        assert len(local_id.override_library.properties) == 0

        # #### Delete the override.
        local_id_name = local_id.name
        assert bpy.data.objects.get((local_id_name, None), None) == local_id
        local_id.override_library.destroy()
        assert bpy.data.objects.get((local_id_name, None), None) is None

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
        assert len(local_id.override_library.properties) == 1
        override_prop = local_id.override_library.properties[0]
        assert override_prop.rna_path == "scale"
        assert len(override_prop.operations) == 1
        override_operation = override_prop.operations[0]
        assert override_operation.operation == 'NOOP'
        assert override_operation.subitem_local_index == -1
        local_id.location.y = 1.0
        local_id.scale.x = 0.5
        # `scale.x` will apply, but will be reverted when the library overrides
        # are updated. This is by design so python scripts can still alter the
        # properties locally what is a typical usecase in productions.
        assert local_id.scale.x == 0.5
        assert local_id.location.y == 1.0

        local_id.override_library.operations_update()
        assert local_id.scale.x == 1.0
        assert local_id.location.y == 1.0

        assert len(local_id.override_library.properties) == 2
        override_prop = local_id.override_library.properties[0]
        assert override_prop.rna_path == "scale"
        assert len(override_prop.operations) == 1
        override_operation = override_prop.operations[0]
        assert override_operation.operation == 'NOOP'
        assert override_operation.subitem_local_index == -1

        override_prop = local_id.override_library.properties[1]
        assert override_prop.rna_path == "location"
        assert len(override_prop.operations) == 1
        override_operation = override_prop.operations[0]
        assert override_operation.operation == 'REPLACE'
        assert override_operation.subitem_local_index == -1


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
        assert obj.override_library is None
        obj.override_template_create()
        assert obj.override_library is not None
        assert len(obj.override_library.properties) == 0
        prop = obj.override_library.properties.add(rna_path='scale')
        assert len(obj.override_library.properties) == 1
        assert len(prop.operations) == 0
        operation = prop.operations.add(operation='NOOP')
        assert len(prop.operations) == 1
        assert operation.operation == 'NOOP'


class TestLibraryOverridesResync(TestHelper, unittest.TestCase):
    DATA_NAME_CONTAINER = "LibCollection"
    DATA_NAME_RIGGED = "LibRigged"
    DATA_NAME_RIG = "LibRig"
    DATA_NAME_CONTROLLER_1 = "LibController1"
    DATA_NAME_CONTROLLER_2 = "LibController2"

    def __init__(self, args):
        self.args = args

        output_dir = pathlib.Path(self.args.output_dir)
        self.ensure_path(str(output_dir))
        self.output_path = output_dir / "blendlib_overrides.blend"
        self.test_output_path = output_dir / "blendlib_overrides_test.blend"

        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)

        collection_container = bpy.data.collections.new(TestLibraryOverridesResync.DATA_NAME_CONTAINER)
        bpy.context.collection.children.link(collection_container)

        mesh = bpy.data.meshes.new(TestLibraryOverridesResync.DATA_NAME_RIGGED)
        obj_child = bpy.data.objects.new(TestLibraryOverridesResync.DATA_NAME_RIGGED, object_data=mesh)
        collection_container.objects.link(obj_child)
        armature = bpy.data.armatures.new(TestLibraryOverridesResync.DATA_NAME_RIG)
        obj_armature = bpy.data.objects.new(TestLibraryOverridesResync.DATA_NAME_RIG, object_data=armature)
        obj_child.parent = obj_armature
        collection_container.objects.link(obj_armature)

        obj_child_modifier = obj_child.modifiers.new("", 'ARMATURE')
        obj_child_modifier.object = obj_armature

        obj_ctrl1 = bpy.data.objects.new(TestLibraryOverridesResync.DATA_NAME_CONTROLLER_1, object_data=None)
        collection_container.objects.link(obj_ctrl1)

        obj_armature_constraint = obj_armature.constraints.new('COPY_LOCATION')
        obj_armature_constraint.target = obj_ctrl1

        collection_sub = bpy.data.collections.new(TestLibraryOverridesResync.DATA_NAME_CONTROLLER_2)
        collection_container.children.link(collection_sub)
        obj_ctrl2 = bpy.data.objects.new(TestLibraryOverridesResync.DATA_NAME_CONTROLLER_2, object_data=None)
        collection_sub.objects.link(obj_ctrl2)

        bpy.ops.wm.save_as_mainfile(filepath=str(self.output_path), check_existing=False, compress=False)

    def test_link_and_override_resync(self):
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        bpy.data.orphans_purge()

        link_dir = self.output_path / "Collection"
        bpy.ops.wm.link(
            directory=str(link_dir),
            filename=TestLibraryOverridesResync.DATA_NAME_CONTAINER,
            instance_collections=False,
        )

        linked_collection_container = bpy.data.collections[TestLibraryOverridesResync.DATA_NAME_CONTAINER]
        assert linked_collection_container.library is not None
        assert linked_collection_container.override_library is None
        assert len(bpy.data.collections) == 2
        assert all(id_.library is not None for id_ in bpy.data.collections)
        assert len(bpy.data.objects) == 4
        assert all(id_.library is not None for id_ in bpy.data.objects)
        assert len(bpy.data.meshes) == 1
        assert all(id_.library is not None for id_ in bpy.data.meshes)
        assert len(bpy.data.armatures) == 1
        assert all(id_.library is not None for id_ in bpy.data.armatures)

        override_collection_container = linked_collection_container.override_hierarchy_create(
            bpy.context.scene,
            bpy.context.view_layer,
        )
        assert override_collection_container.library is None
        assert override_collection_container.override_library is not None
        # Objects and collections are duplicated as overrides, but meshes and armatures remain only linked data.
        assert len(bpy.data.collections) == 4
        assert all((id_.library is None and id_.override_library is not None) for id_ in bpy.data.collections[:2])
        assert len(bpy.data.objects) == 8
        assert all((id_.library is None and id_.override_library is not None) for id_ in bpy.data.objects[:4])
        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.armatures) == 1

        bpy.ops.wm.save_as_mainfile(filepath=str(self.test_output_path), check_existing=False, compress=False)

        # Re-open the lib file, and change its ID relationships.
        bpy.ops.wm.open_mainfile(filepath=str(self.output_path))

        obj_armature = bpy.data.objects[TestLibraryOverridesResync.DATA_NAME_RIG]
        obj_armature_constraint = obj_armature.constraints[0]
        obj_ctrl2 = bpy.data.objects[TestLibraryOverridesResync.DATA_NAME_CONTROLLER_2]
        obj_armature_constraint.target = obj_ctrl2

        bpy.ops.wm.save_as_mainfile(filepath=str(self.output_path), check_existing=False, compress=False)

        # Re-open the main file, and check that automatic resync did its work correctly, remapping the target of the
        # armature constraint to controller 2, without creating unexpected garbage IDs along the line.
        bpy.ops.wm.open_mainfile(filepath=str(self.test_output_path))

        override_collection_container = bpy.data.collections[TestLibraryOverridesResync.DATA_NAME_CONTAINER]
        assert override_collection_container.library is None
        assert override_collection_container.override_library is not None
        # Objects and collections are duplicated as overrides, but meshes and armatures remain only linked data.
        assert len(bpy.data.collections) == 4
        assert all((id_.library is None and id_.override_library is not None) for id_ in bpy.data.collections[:2])
        assert len(bpy.data.objects) == 8
        assert all((id_.library is None and id_.override_library is not None) for id_ in bpy.data.objects[:4])
        assert len(bpy.data.meshes) == 1
        assert len(bpy.data.armatures) == 1

        obj_armature = bpy.data.objects[TestLibraryOverridesResync.DATA_NAME_RIG]
        obj_ctrl2 = bpy.data.objects[TestLibraryOverridesResync.DATA_NAME_CONTROLLER_2]
        assert obj_armature.library is None and obj_armature.override_library is not None
        assert obj_ctrl2.library is None and obj_ctrl2.override_library is not None
        assert obj_armature.constraints[0].target == obj_ctrl2


class TestLibraryOverridesFromProxies(TestHelper, unittest.TestCase):
    # Very basic test, could be improved/extended.
    # NOTE: Tests way more than only liboverride proxy conversion actually, since this is a fairly old .blend file.

    MAIN_BLEND_FILE = "library_test_scene.blend"

    def __init__(self, args):
        self.args = args

        self.test_dir = pathlib.Path(self.args.test_dir)
        self.assertTrue(self.test_dir.exists(),
                        'Test dir {0} should exist'.format(self.test_dir))

        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)

    def test_open_linked_proxy_file(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.test_dir / self.MAIN_BLEND_FILE))

        # Check stability of 'same name' fixing for IDs.
        direct_linked_A = bpy.data.libraries["lib.002"]
        assert direct_linked_A.filepath == os.path.join("//libraries", "direct_linked_A.blend")

        assert bpy.data.objects['HairCubeArmatureGroup_proxy'].library == direct_linked_A
        assert bpy.data.objects['HairCubeArmatureGroup_proxy'].override_library is not None


TESTS = (
    TestLibraryOverrides,
    TestLibraryTemplate,
    TestLibraryOverridesResync,
    TestLibraryOverridesFromProxies,
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
    parser.add_argument(
        "--test-dir",
        dest="test_dir",
        default=".",
        help="Where are the test blendfiles",
        required=False,
    )

    return parser


def main():
    args = argparse_create().parse_args()

    # Don't write thumbnails into the home directory.
    bpy.context.preferences.filepaths.file_preview_type = 'NONE'
    bpy.context.preferences.experimental.use_override_templates = True

    for Test in TESTS:
        Test(args).run_all_tests()


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + \
        (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    main()
