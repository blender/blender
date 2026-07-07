# SPDX-FileCopyrightText: 2021-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --python tests/python/bl_blendfile_library_overrides.py -- --output-dir=/tmp/
import pathlib
import bpy
import sys
import os

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from bl_blendfile_utils import TestHelper


class TestLibraryOverrides(TestHelper):
    MESH_LIBRARY_PARENT = "LibMeshParent"
    OBJECT_LIBRARY_PARENT = "LibMeshParent"
    MESH_LIBRARY_CHILD = "LibMeshChild"
    OBJECT_LIBRARY_CHILD = "LibMeshChild"
    MESH_LIBRARY_PERMISSIVE = "LibMeshPermissive"
    OBJECT_LIBRARY_PERMISSIVE = "LibMeshPermissive"

    def __init__(self, args):
        super().__init__(args)

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
        self.assertEqual(len(local_id.override_library.properties), 0)

        # #### Generate an override property & operation automatically by editing the local override data.
        local_id.location.y = 1.0
        local_id.override_library.operations_update()
        self.assertEqual(len(local_id.override_library.properties), 1)
        override_prop = local_id.override_library.properties[0]
        self.assertEqual(override_prop.rna_path, "location")
        self.assertEqual(len(override_prop.operations), 1)
        override_operation = override_prop.operations[0]
        self.assertEqual(override_operation.operation, 'REPLACE')
        # Setting location.y overrode all elements in the location array. -1 is a wildcard.
        self.assertEqual(override_operation.subitem_local_index, -1)

        # #### Reset the override to its linked reference data.
        local_id.override_library.reset()
        self.assertEqual(len(local_id.override_library.properties), 0)
        self.assertEqual(local_id.location, local_id.override_library.reference.location)

        # #### Generate an override property & operation manually using the API.
        override_property = local_id.override_library.properties.add(rna_path="location")
        override_property.operations.add(operation='REPLACE')

        self.assertEqual(len(local_id.override_library.properties), 1)
        override_prop = local_id.override_library.properties[0]
        self.assertEqual(override_prop.rna_path, "location")
        self.assertEqual(len(override_prop.operations), 1)
        override_operation = override_prop.operations[0]
        self.assertEqual(override_operation.operation, 'REPLACE')
        # Setting location.y overrode all elements in the location array. -1 is a wildcard.
        self.assertEqual(override_operation.subitem_local_index, -1)

        override_property = local_id.override_library.properties[0]
        override_property.operations.remove(override_property.operations[0])
        local_id.override_library.properties.remove(override_property)

        self.assertEqual(len(local_id.override_library.properties), 0)

        # #### Delete the override.
        local_id_name = local_id.name
        self.assertEqual(bpy.data.objects.get((local_id_name, None), None), local_id)
        local_id.override_library.destroy()
        self.assertIsNone(bpy.data.objects.get((local_id_name, None), None))

    def test_link_permissive(self):
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        bpy.data.orphans_purge()

        link_dir = self.output_path / "Object"
        bpy.ops.wm.link(directory=str(link_dir), filename=TestLibraryOverrides.OBJECT_LIBRARY_PERMISSIVE)

        obj = bpy.data.objects[TestLibraryOverrides.OBJECT_LIBRARY_PERMISSIVE]
        self.assertIsNone(obj.override_library)
        local_id = obj.override_create()
        self.assertIsNotNone(local_id.override_library)
        self.assertIsNone(local_id.data.override_library)
        self.assertEqual(len(local_id.override_library.properties), 0)
        local_id.location.y = 1.0
        self.assertEqual(local_id.location.y, 1.0)

        local_id.override_library.operations_update()
        self.assertEqual(local_id.location.y, 1.0)

        self.assertEqual(len(local_id.override_library.properties), 1)
        override_prop = local_id.override_library.properties[0]
        self.assertEqual(override_prop.rna_path, "location")
        self.assertEqual(len(override_prop.operations), 1)
        override_operation = override_prop.operations[0]
        self.assertEqual(override_operation.operation, 'REPLACE')
        self.assertEqual(override_operation.subitem_local_index, -1)


class TestLibraryOverridesComplex(TestHelper):
    # Test resync, recursive resync, overrides of overrides, ID names collision handling, and multiple overrides.

    DATA_NAME_CONTAINER = "LibCollection"
    DATA_NAME_SUBCONTAINER_0 = "LibSubCollection_0"
    DATA_NAME_SUBCONTAINER_1 = "LibSubCollection_1"
    DATA_NAME_SUBCONTAINER_2 = "LibSubCollection_2"
    DATA_NAME_RIGGED = "LibRigged"
    DATA_NAME_RIG = "LibRig"
    DATA_NAME_CONTROLLER_1 = "LibController1"
    DATA_NAME_CONTROLLER_2 = "LibController2"
    DATA_NAME_SAMENAME_CONTAINER = "LibCube"
    DATA_NAME_SAMENAME_0 = "LibCube"
    DATA_NAME_SAMENAME_1 = "LibCube.001"
    DATA_NAME_SAMENAME_2 = "LibCube.002"
    DATA_NAME_SAMENAME_3 = "LibCube.003"

    def __init__(self, args):
        super().__init__(args)

        output_dir = pathlib.Path(self.args.output_dir)
        self.ensure_path(str(output_dir))
        self.lib_output_path = output_dir / "blendlib_overrides_lib.blend"
        self.test_output_path = output_dir / "blendlib_overrides_test.blend"
        self.test_output_path_recursive = output_dir / "blendlib_overrides_test_recursive.blend"

    def reset(self):
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)

    def init_lib_data(self, custom_cb=None):
        self.reset()

        collection_container = bpy.data.collections.new(self.__class__.DATA_NAME_CONTAINER)
        bpy.context.collection.children.link(collection_container)

        mesh = bpy.data.meshes.new(self.__class__.DATA_NAME_RIGGED)
        obj_child = bpy.data.objects.new(self.__class__.DATA_NAME_RIGGED, object_data=mesh)
        collection_container.objects.link(obj_child)
        armature = bpy.data.armatures.new(self.__class__.DATA_NAME_RIG)
        obj_armature = bpy.data.objects.new(self.__class__.DATA_NAME_RIG, object_data=armature)
        obj_child.parent = obj_armature
        collection_container.objects.link(obj_armature)

        obj_child_modifier = obj_child.modifiers.new("", 'ARMATURE')
        obj_child_modifier.object = obj_armature

        obj_ctrl1 = bpy.data.objects.new(self.__class__.DATA_NAME_CONTROLLER_1, object_data=None)
        collection_container.objects.link(obj_ctrl1)

        obj_armature_constraint = obj_armature.constraints.new('COPY_LOCATION')
        obj_armature_constraint.target = obj_ctrl1

        collection_sub = bpy.data.collections.new(self.__class__.DATA_NAME_CONTROLLER_2)
        collection_container.children.link(collection_sub)
        obj_ctrl2 = bpy.data.objects.new(self.__class__.DATA_NAME_CONTROLLER_2, object_data=None)
        collection_sub.objects.link(obj_ctrl2)

        collection_sub = bpy.data.collections.new(self.__class__.DATA_NAME_SAMENAME_CONTAINER)
        collection_container.children.link(collection_sub)
        # 'Samename' objects are purposely not added to the collection here.

        # Sub-container collections are empty by default.
        collection_subcontainer_0 = bpy.data.collections.new(self.__class__.DATA_NAME_SUBCONTAINER_0)
        collection_container.children.link(collection_subcontainer_0)
        collection_subcontainer_1 = bpy.data.collections.new(self.__class__.DATA_NAME_SUBCONTAINER_1)
        collection_container.children.link(collection_subcontainer_1)
        collection_subcontainer_2 = bpy.data.collections.new(self.__class__.DATA_NAME_SUBCONTAINER_2)
        collection_container.children.link(collection_subcontainer_2)

        if custom_cb is not None:
            custom_cb(self)

        bpy.ops.wm.save_as_mainfile(
            filepath=str(self.lib_output_path),
            check_existing=False,
            compress=False,
            relative_remap=False,
        )

    def edit_lib_data(self, custom_cb):
        bpy.ops.wm.open_mainfile(filepath=str(self.lib_output_path))
        custom_cb(self)
        bpy.ops.wm.save_as_mainfile(
            filepath=str(self.lib_output_path),
            check_existing=False,
            compress=False,
            relative_remap=False,
        )

    def link_lib_data(self, num_collections, num_objects, num_meshes, num_armatures):
        link_dir = self.lib_output_path / "Collection"
        bpy.ops.wm.link(
            directory=str(link_dir),
            filename=self.__class__.DATA_NAME_CONTAINER,
            instance_collections=False,
            relative_path=False,
        )

        linked_collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER]

        self.assertIsNotNone(linked_collection_container.library)
        self.assertIsNone(linked_collection_container.override_library)
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, num_collections[0], num_collections[1])
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, num_objects[0], num_objects[1])
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, num_meshes[0], num_meshes[1])
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, num_armatures[0], num_armatures[1])

        return linked_collection_container

    def link_liboverride_data(self, num_collections, num_objects, num_meshes, num_armatures):
        link_dir = self.test_output_path / "Collection"
        bpy.ops.wm.link(
            directory=str(link_dir),
            filename=self.__class__.DATA_NAME_CONTAINER,
            instance_collections=False,
            relative_path=False,
        )

        linked_collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER, str(
            self.test_output_path)]
        self.assertIsNotNone(linked_collection_container.library)
        self.assertIsNotNone(linked_collection_container.override_library)
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, num_collections[0], num_collections[1])
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, num_objects[0], num_objects[1])
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, num_meshes[0], num_meshes[1])
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, num_armatures[0], num_armatures[1])

        self.liboverride_hierarchy_validate(linked_collection_container)

        return linked_collection_container

    def liboverride_hierarchy_validate(self, root_collection):
        def liboverride_systemoverrideonly_hierarchy_validate(id_, id_root):
            if not id_.override_library:
                return
            self.assertEqual(id_.override_library.hierarchy_root, id_root)
            for op in id_.override_library.properties:
                for opop in op.operations:
                    self.assertIn('IDPOINTER_MATCH_REFERENCE', opop.flag)

        for coll_ in root_collection.children_recursive:
            liboverride_systemoverrideonly_hierarchy_validate(coll_, root_collection)
            if coll_.override_library:
                for op in coll_.override_library.properties:
                    for opop in op.operations:
                        self.assertIn('IDPOINTER_ITEM_USE_ID', opop.flag)
                        print(
                            coll_,
                            opop.flag,
                            opop.subitem_reference_name,
                            opop.subitem_reference_id,
                            opop.subitem_local_name,
                            opop.subitem_local_id)
                        self.assertIsNotNone(opop.subitem_reference_id.library)
                        self.assertTrue(opop.subitem_local_id.library is None if coll_.library is None
                                        else opop.subitem_local_id.library is not None)
        for ob_ in root_collection.all_objects:
            liboverride_systemoverrideonly_hierarchy_validate(ob_, root_collection)

    # IDs container, number of local IDs, of linked IDs, and number of liboverrides (local, linked).
    def local_and_linked_ids_numbers_validate(
            self, ids, local_num, linked_num, linked_missing_num=..., liboverride_num=()):
        self.assertEqual(len(ids), local_num + linked_num)
        self.assertFalse(any(id_.library is not None for id_ in ids[:local_num]))
        self.assertFalse(any(id_.library is None for id_ in ids[local_num:]))
        if liboverride_num:
            self.assertEqual(len([id_ for id_ in ids[:local_num] if id_.override_library is not None]),
                             liboverride_num[0])
            self.assertEqual(len([id_ for id_ in ids[local_num:] if id_.override_library is not None]),
                             liboverride_num[1])
        if linked_missing_num is not ...:
            self.assertEqual(len([id_ for id_ in ids[local_num:] if id_.is_missing]), linked_missing_num)

    def test_link_and_override_resync(self):
        self.init_lib_data()
        self.reset()

        # NOTE: All counts below are in the form `local_ids, linked_ids`.
        linked_collection_container = self.link_lib_data(
            num_collections=(0, 6),
            num_objects=(0, 4),
            num_meshes=(0, 1),
            num_armatures=(0, 1))

        override_collection_container = linked_collection_container.override_hierarchy_create(
            bpy.context.scene,
            bpy.context.view_layer,
        )
        self.assertIsNone(override_collection_container.library)
        self.assertIsNotNone(override_collection_container.override_library)
        # Objects and collections are duplicated as overrides (except for empty collection),
        # but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 2, 6, liboverride_num=(2, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 4, 4, liboverride_num=(4, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        self.liboverride_hierarchy_validate(override_collection_container)

        bpy.ops.wm.save_as_mainfile(
            filepath=str(self.test_output_path),
            check_existing=False,
            compress=False,
            relative_remap=False,
        )

        # Create linked liboverrides file (for recursive resync).
        self.reset()

        self.link_liboverride_data(
            num_collections=(0, 8),
            num_objects=(0, 8),
            num_meshes=(0, 1),
            num_armatures=(0, 1))

        bpy.ops.wm.save_as_mainfile(
            filepath=str(self.test_output_path_recursive),
            check_existing=False,
            compress=False,
            relative_remap=False,
        )

        # Re-open the lib file, and change its ID relationships.
        bpy.ops.wm.open_mainfile(filepath=str(self.lib_output_path))

        obj_armature = bpy.data.objects[self.__class__.DATA_NAME_RIG]
        obj_armature_constraint = obj_armature.constraints[0]
        obj_ctrl2 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_2]
        obj_armature_constraint.target = obj_ctrl2

        bpy.ops.wm.save_as_mainfile(filepath=str(self.lib_output_path), check_existing=False, compress=False)

        # Re-open the main file, and check that automatic resync did its work correctly, remapping the target of the
        # armature constraint to controller 2, without creating unexpected garbage IDs along the line.
        bpy.ops.wm.open_mainfile(filepath=str(self.test_output_path))

        override_collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER]
        self.assertIsNone(override_collection_container.library)
        self.assertIsNotNone(override_collection_container.override_library)
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 2, 6, liboverride_num=(2, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 4, 4, liboverride_num=(4, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        obj_armature = bpy.data.objects[self.__class__.DATA_NAME_RIG]
        obj_ctrl2 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_2]
        self.assertIsNone(obj_armature.library)
        self.assertIsNotNone(obj_armature.override_library)
        self.assertIsNone(obj_ctrl2.library)
        self.assertIsNotNone(obj_ctrl2.override_library)
        self.assertEqual(obj_armature.constraints[0].target, obj_ctrl2)

        self.liboverride_hierarchy_validate(override_collection_container)

        # Re-open the 'recursive resync' file, and check that automatic recursive resync did its work correctly,
        # remapping the target of the linked liboverride armature constraint to controller 2, without creating
        # unexpected garbage IDs along the line.
        bpy.ops.wm.open_mainfile(filepath=str(self.test_output_path_recursive))

        override_collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER, str(
            self.test_output_path)]
        self.assertIsNotNone(override_collection_container.library)
        self.assertIsNotNone(override_collection_container.override_library)
        test_output_path_lib = override_collection_container.library
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 0, 2 + 6, liboverride_num=(0, 2))
        self.assertTrue(all((id_.override_library is not None)
                            for id_ in bpy.data.collections if id_.library == test_output_path_lib))
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 0, 4 + 4, liboverride_num=(0, 4))
        self.assertTrue(all((id_.override_library is not None)
                            for id_ in bpy.data.objects if id_.library == test_output_path_lib))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        obj_armature = bpy.data.objects[self.__class__.DATA_NAME_RIG, str(self.test_output_path)]
        obj_ctrl2 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_2, str(self.test_output_path)]
        self.assertIsNotNone(obj_armature.override_library)
        self.assertIsNotNone(obj_ctrl2.override_library)
        self.assertEqual(obj_armature.constraints[0].target, obj_ctrl2)

        self.liboverride_hierarchy_validate(override_collection_container)

    def test_link_and_override_multiple(self):
        self.init_lib_data()
        self.reset()

        # NOTE: All counts below are in the form `local_ids, linked_ids`.
        linked_collection_container = self.link_lib_data(
            num_collections=(0, 6),
            num_objects=(0, 4),
            num_meshes=(0, 1),
            num_armatures=(0, 1))

        override_collection_containers = [linked_collection_container.override_hierarchy_create(
            bpy.context.scene,
            bpy.context.view_layer,
        ) for i in range(3)]
        for override_container in override_collection_containers:
            self.assertIsNone(override_container.library)
            self.assertIsNotNone(override_container.override_library)
            self.liboverride_hierarchy_validate(override_container)

        # Objects and collections are duplicated as overrides (except for empty collection),
        # but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 3 * 2, 6, liboverride_num=(3 * 2, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 3 * 4, 4, liboverride_num=(3 * 4, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        bpy.ops.wm.save_as_mainfile(
            filepath=str(self.test_output_path),
            check_existing=False,
            compress=False,
            relative_remap=False,
        )

        # Create linked liboverrides file (for recursive resync).
        self.reset()

        self.link_liboverride_data(
            num_collections=(0, 8),
            num_objects=(0, 8),
            num_meshes=(0, 1),
            num_armatures=(0, 1))

        bpy.ops.wm.save_as_mainfile(
            filepath=str(self.test_output_path_recursive),
            check_existing=False,
            compress=False,
            relative_remap=False,
        )

        # Change the lib's ID relationships.
        def edit_lib_cb(self):
            obj_armature = bpy.data.objects[self.__class__.DATA_NAME_RIG]
            obj_armature_constraint = obj_armature.constraints[0]
            obj_ctrl2 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_2]
            obj_armature_constraint.target = obj_ctrl2
        self.edit_lib_data(edit_lib_cb)

        # Re-open the main file, and check that automatic resync did its work correctly, remapping the target of the
        # armature constraint to controller 2, without creating unexpected garbage IDs along the line.
        bpy.ops.wm.open_mainfile(filepath=str(self.test_output_path))

        override_collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER]
        self.assertIsNone(override_collection_container.library)
        self.assertIsNotNone(override_collection_container.override_library)
        # Objects and collections are duplicated as overrides, but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 3 * 2, 6, liboverride_num=(3 * 2, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 3 * 4, 4, liboverride_num=(3 * 4, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        obj_armature = bpy.data.objects[self.__class__.DATA_NAME_RIG]
        obj_ctrl2 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_2]
        self.assertIsNotNone(obj_armature.library is None and obj_armature.override_library)
        self.assertIsNotNone(obj_ctrl2.library is None and obj_ctrl2.override_library)
        self.assertEqual(obj_armature.constraints[0].target, obj_ctrl2)

        override_collection_containers = [
            bpy.data.collections[self.__class__.DATA_NAME_CONTAINER],
            bpy.data.collections[self.__class__.DATA_NAME_CONTAINER + ".001"],
            bpy.data.collections[self.__class__.DATA_NAME_CONTAINER + ".002"],
        ]
        for override_container in override_collection_containers:
            self.assertIsNone(override_container.library)
            self.assertIsNotNone(override_container.override_library)
            self.liboverride_hierarchy_validate(override_container)

        # Re-open the 'recursive resync' file, and check that automatic recursive resync did its work correctly,
        # remapping the target of the linked liboverride armature constraint to controller 2, without creating
        # unexpected garbage IDs along the line.
        bpy.ops.wm.open_mainfile(filepath=str(self.test_output_path_recursive))

        linked_collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER, str(
            self.test_output_path)]
        self.assertIsNotNone(linked_collection_container.library)
        self.assertIsNotNone(linked_collection_container.override_library)
        test_output_path_lib = linked_collection_container.library
        # Objects and collections are duplicated as overrides, but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 0, 2 + 6, liboverride_num=(0, 2))
        self.assertTrue(all((id_.override_library is not None)
                            for id_ in bpy.data.collections if id_.library == test_output_path_lib))
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 0, 4 + 4, liboverride_num=(0, 4))
        self.assertTrue(all((id_.override_library is not None)
                            for id_ in bpy.data.objects if id_.library == test_output_path_lib))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        obj_armature = bpy.data.objects[self.__class__.DATA_NAME_RIG, str(self.test_output_path)]
        obj_ctrl2 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_2, str(self.test_output_path)]
        self.assertIsNotNone(obj_armature.override_library)
        self.assertIsNotNone(obj_ctrl2.override_library)
        self.assertEqual(obj_armature.constraints[0].target, obj_ctrl2)

        self.liboverride_hierarchy_validate(linked_collection_container)

    def test_link_and_override_of_override(self):
        def init_lib_cb(self):
            collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER]
            # Sub-container collections become a non-empty, multi-level hierarchy.
            collection_subcontainer_0 = bpy.data.collections[self.__class__.DATA_NAME_SUBCONTAINER_0]
            collection_subcontainer_1 = bpy.data.collections[self.__class__.DATA_NAME_SUBCONTAINER_1]
            collection_subcontainer_2 = bpy.data.collections[self.__class__.DATA_NAME_SUBCONTAINER_2]
            collection_container.children.unlink(collection_subcontainer_2)
            collection_subcontainer_0.children.link(collection_subcontainer_2)

            obj_ctrl1 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_1]
            collection_container.objects.unlink(obj_ctrl1)
            collection_subcontainer_2.objects.link(obj_ctrl1)

            # Now obj_ctrl2 is both in its (original) sub-container conllection, and in collection_subcontainer_1.
            obj_ctrl2 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_2]
            collection_subcontainer_1.objects.link(obj_ctrl2)

        self.init_lib_data(init_lib_cb)
        self.reset()

        # NOTE: All counts below are in the form `local_ids, linked_ids`.
        linked_collection_container = self.link_lib_data(
            num_collections=(0, 6),
            num_objects=(0, 4),
            num_meshes=(0, 1),
            num_armatures=(0, 1))

        override_collection_container = linked_collection_container.override_hierarchy_create(
            bpy.context.scene,
            bpy.context.view_layer,
        )
        self.assertIsNone(override_collection_container.library)
        self.assertIsNotNone(override_collection_container.override_library)

        # Objects and collections are duplicated as overrides (except for empty collection),
        # but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 5, 6, liboverride_num=(5, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 4, 4, liboverride_num=(4, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        self.liboverride_hierarchy_validate(override_collection_container)

        bpy.ops.wm.save_as_mainfile(
            filepath=str(self.test_output_path),
            check_existing=False,
            compress=False,
            relative_remap=False,
        )

        # Create liboverrides of liboverrides file.
        self.reset()

        linked_collection_container = self.link_liboverride_data(
            num_collections=(0, 11),
            num_objects=(0, 8),
            num_meshes=(0, 1),
            num_armatures=(0, 1))

        override_collection_container = linked_collection_container.override_hierarchy_create(
            bpy.context.scene,
            bpy.context.view_layer,
        )
        self.assertIsNone(override_collection_container.library)
        self.assertIsNotNone(override_collection_container.override_library)

        # Objects and collections are duplicated as overrides (except for empty collection),
        # but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 5, 5 + 6, liboverride_num=(5, 5))
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 4, 4 + 4, liboverride_num=(4, 4))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        self.liboverride_hierarchy_validate(override_collection_container)

        bpy.ops.wm.save_as_mainfile(
            filepath=str(self.test_output_path_recursive),
            check_existing=False,
            compress=False,
            relative_remap=False,
        )

        # Edit the lib file, change its ID relationships.
        def edit_lib_cb(self):
            obj_armature = bpy.data.objects[self.__class__.DATA_NAME_RIG]
            obj_armature_constraint = obj_armature.constraints[0]
            obj_ctrl2 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_2]
            obj_armature_constraint.target = obj_ctrl2

            # Also modify sub-container collection hierarchies.
            collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER]
            # Sub-container 1 is moved from collection_container to sub-container 0.
            collection_subcontainer_0 = bpy.data.collections[self.__class__.DATA_NAME_SUBCONTAINER_0]
            collection_subcontainer_1 = bpy.data.collections[self.__class__.DATA_NAME_SUBCONTAINER_1]
            collection_container.children.unlink(collection_subcontainer_1)
            collection_subcontainer_0.children.link(collection_subcontainer_1)

        self.edit_lib_data(edit_lib_cb)
        self.reset()

        # Re-open the main file, and check that automatic resync did its work correctly, remapping the target of the
        # armature constraint to controller 2, without creating unexpected garbage IDs along the line.
        bpy.ops.wm.open_mainfile(filepath=str(self.test_output_path))

        override_collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER]
        self.assertIsNone(override_collection_container.library)
        self.assertIsNotNone(override_collection_container.override_library)
        # Objects and collections are duplicated as overrides, but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 5, 6, liboverride_num=(5, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 4, 4, liboverride_num=(4, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        obj_armature = bpy.data.objects[self.__class__.DATA_NAME_RIG]
        obj_ctrl2 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_2]
        self.assertIsNone(obj_armature.library)
        self.assertIsNotNone(obj_armature.override_library)
        self.assertIsNone(obj_ctrl2.library)
        self.assertIsNotNone(obj_ctrl2.override_library)
        self.assertEqual(obj_armature.constraints[0].target, obj_ctrl2)

        self.liboverride_hierarchy_validate(override_collection_container)

        bpy.ops.wm.save_as_mainfile(
            filepath=str(self.test_output_path),
            check_existing=False,
            compress=False,
            relative_remap=False,
        )

        # Re-open the 'recursive resync' file, and check that automatic recursive resync did its work correctly,
        # remapping the target of the linked liboverride armature constraint to controller 2, without creating
        # unexpected garbage IDs along the line.
        bpy.ops.wm.open_mainfile(filepath=str(self.test_output_path_recursive))

        override_collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER]
        self.assertIsNone(override_collection_container.library)
        self.assertIsNotNone(override_collection_container.override_library)
        # Objects and collections are duplicated as overrides, but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 5, 5 + 6, liboverride_num=(5, 5))
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 4, 4 + 4, liboverride_num=(4, 4))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        obj_armature = bpy.data.objects[self.__class__.DATA_NAME_RIG]
        obj_ctrl2 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_2]
        self.assertIsNotNone(obj_armature.override_library)
        self.assertIsNotNone(obj_ctrl2.override_library)
        self.assertEqual(obj_armature.constraints[0].target, obj_ctrl2)

        self.liboverride_hierarchy_validate(override_collection_container)

        bpy.ops.wm.save_as_mainfile(
            filepath=str(self.test_output_path_recursive),
            check_existing=False,
            compress=False,
            relative_remap=False,
        )

        # Clear the library from all of its data.
        self.edit_lib_data(lambda s: s.reset())
        self.reset()

        # Re-open the main file, and check that automatic resync did its work correctly, preserving as much as possible
        # from the missing linked data.
        bpy.ops.wm.open_mainfile(filepath=str(self.test_output_path))

        override_collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER]
        self.assertIsNone(override_collection_container.library)
        self.assertIsNotNone(override_collection_container.override_library)
        # Objects and collections are duplicated as overrides, but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(
            bpy.data.collections, 5, 6, linked_missing_num=6, liboverride_num=(5, 0))
        self.local_and_linked_ids_numbers_validate(
            bpy.data.objects, 4, 4, linked_missing_num=4, liboverride_num=(4, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        obj_armature = bpy.data.objects[self.__class__.DATA_NAME_RIG]
        obj_ctrl2 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_2]
        self.assertIsNone(obj_armature.library)
        self.assertIsNotNone(obj_armature.override_library)
        self.assertIsNone(obj_ctrl2.library)
        self.assertIsNotNone(obj_ctrl2.override_library)
        print(obj_armature.constraints[0].target)
        self.assertEqual(obj_armature.constraints[0].target, obj_ctrl2)

        self.liboverride_hierarchy_validate(override_collection_container)

        # Re-open the 'recursive resync' file, and check that automatic recursive resync did its work correctly,
        # remapping the target of the linked liboverride armature constraint to controller 2, without creating
        # unexpected garbage IDs along the line.
        bpy.ops.wm.open_mainfile(filepath=str(self.test_output_path_recursive))

        override_collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER]
        self.assertIsNone(override_collection_container.library)
        self.assertIsNotNone(override_collection_container.override_library)
        # Objects and collections are duplicated as overrides, but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(
            bpy.data.collections, 5, 5 + 6, linked_missing_num=6, liboverride_num=(5, 5))
        self.local_and_linked_ids_numbers_validate(
            bpy.data.objects, 4, 4 + 4, linked_missing_num=4, liboverride_num=(4, 4))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        obj_armature = bpy.data.objects[self.__class__.DATA_NAME_RIG]
        obj_ctrl2 = bpy.data.objects[self.__class__.DATA_NAME_CONTROLLER_2]
        self.assertIsNotNone(obj_armature.override_library)
        self.assertIsNotNone(obj_ctrl2.override_library)
        self.assertEqual(obj_armature.constraints[0].target, obj_ctrl2)

        self.liboverride_hierarchy_validate(override_collection_container)

    def test_link_and_override_idnames_conflict(self):
        def init_lib_cb(self):
            # Add some 'samename' objects to the library.
            collection_sub = bpy.data.collections[self.__class__.DATA_NAME_SAMENAME_CONTAINER]
            obj_samename_0 = bpy.data.objects.new(self.__class__.DATA_NAME_SAMENAME_0, object_data=None)
            collection_sub.objects.link(obj_samename_0)
            obj_samename_3 = bpy.data.objects.new(self.__class__.DATA_NAME_SAMENAME_3, object_data=None)
            collection_sub.objects.link(obj_samename_3)
        self.init_lib_data(init_lib_cb)
        self.reset()

        # NOTE: All counts below are in the form `local_ids, linked_ids`.
        linked_collection_container = self.link_lib_data(
            num_collections=(0, 6),
            num_objects=(0, 6),
            num_meshes=(0, 1),
            num_armatures=(0, 1))

        override_collection_containers = [linked_collection_container.override_hierarchy_create(
            bpy.context.scene,
            bpy.context.view_layer,
        ) for i in range(3)]
        for override_container in override_collection_containers:
            self.assertIsNone(override_container.library)
            self.assertIsNotNone(override_container.override_library)
            self.liboverride_hierarchy_validate(override_container)

        # Objects and collections are duplicated as overrides (except for empty collection),
        # but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 3 * 3, 6, liboverride_num=(3 * 3, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 3 * 6, 6, liboverride_num=(3 * 6, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        self.assertEqual(
            bpy.data.objects[self.__class__.DATA_NAME_SAMENAME_0].override_library.reference.name,
            self.__class__.DATA_NAME_SAMENAME_0)
        self.assertEqual(
            bpy.data.objects[self.__class__.DATA_NAME_SAMENAME_3].override_library.reference.name,
            self.__class__.DATA_NAME_SAMENAME_3)
        # These names will be used by the second created liboverride, due to how
        # naming is currently handled when original name is already used.
        self.assertEqual(
            bpy.data.objects[self.__class__.DATA_NAME_SAMENAME_1].override_library.reference.name,
            self.__class__.DATA_NAME_SAMENAME_0)
        self.assertEqual(
            bpy.data.objects[self.__class__.DATA_NAME_SAMENAME_2].override_library.reference.name,
            self.__class__.DATA_NAME_SAMENAME_3)

        bpy.ops.wm.save_as_mainfile(
            filepath=str(self.test_output_path),
            check_existing=False,
            compress=False,
            relative_remap=False,
        )

        # Create liboverrides of liboverrides file.
        self.reset()

        linked_collection_container = self.link_liboverride_data(
            num_collections=(0, 9),
            num_objects=(0, 12),
            num_meshes=(0, 1),
            num_armatures=(0, 1))

        override_collection_container = linked_collection_container.override_hierarchy_create(
            bpy.context.scene,
            bpy.context.view_layer,
        )
        self.assertIsNone(override_collection_container.library)
        self.assertIsNotNone(override_collection_container.override_library)

        # Objects and collections are duplicated as overrides (except for empty collection),
        # but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 3, 3 + 6, liboverride_num=(3, 3))
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 6, 6 + 6, liboverride_num=(6, 6))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        self.liboverride_hierarchy_validate(override_collection_container)

        bpy.ops.wm.save_as_mainfile(
            filepath=str(self.test_output_path_recursive),
            check_existing=False,
            compress=False,
            relative_remap=False,
        )

        # Modify the names of 'samename' objects in the library to generate ID name collisions.
        def edit_lib_cb(self):
            obj_samename_0 = bpy.data.objects[self.__class__.DATA_NAME_SAMENAME_0]
            obj_samename_3 = bpy.data.objects[self.__class__.DATA_NAME_SAMENAME_3]
            obj_samename_0.name = self.__class__.DATA_NAME_SAMENAME_2
            obj_samename_3.name = self.__class__.DATA_NAME_SAMENAME_1
        self.edit_lib_data(edit_lib_cb)

        # Re-open the main file, and check that automatic resync did its work correctly, remapping the target of the
        # armature constraint to controller 2, without creating unexpected garbage IDs along the line.
        bpy.ops.wm.open_mainfile(filepath=str(self.test_output_path))

        override_collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER]
        self.assertIsNone(override_collection_container.library)
        self.assertIsNotNone(override_collection_container.override_library)
        # Objects and collections are duplicated as overrides, but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 3 * 3, 6, liboverride_num=(3 * 3, 0))
        # Note that the 'missing' renamed objects from the library are now cleared as part of the resync process.
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 3 * 6, 6, liboverride_num=(3 * 6, 0))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        override_collection_containers = [
            bpy.data.collections[self.__class__.DATA_NAME_CONTAINER],
            bpy.data.collections[self.__class__.DATA_NAME_CONTAINER + ".001"],
            bpy.data.collections[self.__class__.DATA_NAME_CONTAINER + ".002"],
        ]
        for override_container in override_collection_containers:
            self.assertIsNone(override_container.library)
            self.assertIsNotNone(override_container.override_library)
            self.liboverride_hierarchy_validate(override_container)

        # Re-open the 'recursive resync' file, and check that automatic recursive resync did its work correctly,
        # remapping the target of the linked liboverride armature constraint to controller 2, without creating
        # unexpected garbage IDs along the line.
        bpy.ops.wm.open_mainfile(filepath=str(self.test_output_path_recursive))

        linked_collection_container = bpy.data.collections[self.__class__.DATA_NAME_CONTAINER, str(
            self.test_output_path)]
        self.assertIsNotNone(linked_collection_container.library)
        self.assertIsNotNone(linked_collection_container.override_library)

        test_output_path_lib = linked_collection_container.library
        # Objects and collections are duplicated as overrides, but meshes and armatures remain only linked data.
        self.local_and_linked_ids_numbers_validate(bpy.data.collections, 3, 3 + 6, liboverride_num=(3, 3))
        # Note that the 'missing' renamed objects from the library are now cleared as part of the resync process.
        self.local_and_linked_ids_numbers_validate(bpy.data.objects, 6, 6 + 6, liboverride_num=(6, 6))
        self.local_and_linked_ids_numbers_validate(bpy.data.meshes, 0, 1)
        self.local_and_linked_ids_numbers_validate(bpy.data.armatures, 0, 1)

        self.liboverride_hierarchy_validate(linked_collection_container)


class TestLibraryOverridesFromProxies(TestHelper):
    # Very basic test, could be improved/extended.
    # NOTE: Tests way more than only liboverride proxy conversion actually, since this is a fairly old .blend file.

    MAIN_BLEND_FILE = "library_test_scene.blend"

    def __init__(self, args):
        super().__init__(args)

        self.test_dir = pathlib.Path(self.args.test_dir)
        self.assertTrue(self.test_dir.exists(),
                        msg='Test dir {0} should exist'.format(self.test_dir))

        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)

    def test_open_linked_proxy_file(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.test_dir / self.MAIN_BLEND_FILE))

        # Check stability of 'same name' fixing for IDs.
        direct_linked_A = bpy.data.libraries["lib.002"]
        self.assertEqual(direct_linked_A.filepath, os.path.join("//libraries", "direct_linked_A.blend"))

        self.assertEqual(bpy.data.objects['HairCubeArmatureGroup_proxy'].library, direct_linked_A)
        self.assertIsNotNone(bpy.data.objects['HairCubeArmatureGroup_proxy'].override_library)


TESTS = (
    TestLibraryOverrides,
    TestLibraryOverridesComplex,
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

    for Test in TESTS:
        Test(args).run_all_tests()


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + \
        (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    main()
