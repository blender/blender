# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys
import tempfile
import unittest

import bpy


args = None


class CollectionIOTestBase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir

    def setUp(self):
        self._tempdir = tempfile.TemporaryDirectory()
        self.tempdir = pathlib.Path(self._tempdir.name)

        self.assertTrue(self.testdir.exists(),
                        'Test dir {0} should exist'.format(self.testdir))
        self.assertTrue(self.tempdir.exists(),
                        'Temp dir {0} should exist'.format(self.tempdir))

    def tearDown(self):
        self._tempdir.cleanup()

    @staticmethod
    def reset_blender():
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        bpy.data.orphans_purge(do_recursive=True)

    @staticmethod
    def create_collections(parent_collection, collection_names):
        for collection_name in collection_names:
            coll = bpy.data.collections.new(collection_name)
            parent_collection.children.link(coll)

    @staticmethod
    def find_layer_collection(layer_coll, collection_name):
        if layer_coll.collection.name == collection_name:
            return layer_coll
        for child in layer_coll.children:
            found = CollectionIOTestBase.find_layer_collection(child, collection_name)
            if found:
                return found
        return None

    @staticmethod
    def copy_file(path_src, path_dst):
        import shutil
        shutil.copy(path_src, path_dst)

    def set_active_collection(self, collection_name):
        lc_main = bpy.context.view_layer.layer_collection
        lc_target = CollectionIOTestBase.find_layer_collection(lc_main, collection_name)
        self.assertIsNotNone(lc_target, f"Could not find layer collection for {collection_name}")

        bpy.context.view_layer.active_layer_collection = lc_target

    def add_collection_importer(self, collection_name, importer_type, expected_result={'FINISHED'}):
        self.set_active_collection(collection_name)

        res = bpy.ops.collection.importer_add(name=importer_type)
        self.assertEqual(res, expected_result)
        self.assertIsNotNone(
            bpy.data.collections[collection_name].importer,
            f"Failed to add {importer_type} importer on collection {collection_name}")

    def remove_collection_importer(self, collection_name, expected_result={'FINISHED'}):
        self.set_active_collection(collection_name)

        res = bpy.ops.collection.importer_remove()
        self.assertEqual(res, expected_result)
        self.assertIsNone(
            bpy.data.collections[collection_name].importer,
            f"Failed to remove importer on collection {collection_name}")

    def reload_collection_importer(self, collection_name):
        self.set_active_collection(collection_name)

        bpy.ops.collection.importer_reload()
        self.assertIsNotNone(
            bpy.data.collections[collection_name].importer,
            f"Reload incorrectly removed importer on collection {collection_name}")

    def clear_collection_importer(self, collection_name):
        self.set_active_collection(collection_name)

        bpy.ops.collection.importer_clear()
        self.assertIsNotNone(
            bpy.data.collections[collection_name].importer,
            f"Clear incorrectly removed importer on collection {collection_name}")

    def make_local_collection(self, collection_name):
        self.set_active_collection(collection_name)

        bpy.ops.collection.importer_make_local()
        self.assertIsNone(
            bpy.data.collections[collection_name].importer,
            f"Make local failed to remove importer on collection {collection_name}")

    def do_collection_import(self, collection_name):
        self.set_active_collection(collection_name)

        bpy.ops.collection.importer_import()


class TestCollectionImport(CollectionIOTestBase):

    def __init__(self, args):
        super().__init__(args)

    def test_add_remove(self):
        # Validate behavior of adding and removing importers
        self.reset_blender()

        coll_A = "CollectionA"
        coll_B = "CollectionB"
        coll_main = bpy.context.scene.collection
        self.create_collections(coll_main, (coll_A, coll_B))

        self.add_collection_importer(coll_A, "IO_FH_usd")
        self.add_collection_importer(coll_B, "IO_FH_usd")
        with self.assertRaises(RuntimeError, msg=f"Adding a second importer to {coll_B} should fail"):
            self.add_collection_importer(coll_B, "IO_FH_usd")

        self.remove_collection_importer(coll_A)
        self.remove_collection_importer(coll_B)
        with self.assertRaises(RuntimeError, msg=f"Removing a non-existant importer from {coll_B} should fail"):
            self.remove_collection_importer(coll_B, "IO_FH_usd")

    def test_import(self):
        # Validate basic import functionality.
        self.reset_blender()

        coll_A = "CollectionA"
        coll_B = "CollectionB"
        coll_C = "CollectionC"
        coll_main = bpy.context.scene.collection
        self.create_collections(coll_main, (coll_A, coll_B, coll_C))

        self.add_collection_importer(coll_A, "IO_FH_usd")
        self.add_collection_importer(coll_B, "IO_FH_usd")
        self.add_collection_importer(coll_C, "IO_FH_usd")

        # Setup each importer using a unique external file
        self.copy_file(self.testdir / "import-default.usda", self.tempdir / "file1.usda")
        self.copy_file(self.testdir / "import-default.usda", self.tempdir / "file2.usda")
        self.copy_file(self.testdir / "import-default.usda", self.tempdir / "file3.usda")

        coll = bpy.data.collections[coll_A]
        coll.importer.filepath = str(self.tempdir / "file1.usda")

        coll = bpy.data.collections[coll_B]
        coll.importer.filepath = str(self.tempdir / "file2.usda")
        coll.importer.import_properties.prim_path_mask = "/root/Cube"

        coll = bpy.data.collections[coll_C]
        coll.importer.filepath = str(self.tempdir / "file3.usda")
        coll.importer.import_properties.prim_path_mask = "/root/does_not_exist"

        # Import and validate
        self.do_collection_import(coll_A)
        self.do_collection_import(coll_B)
        self.do_collection_import(coll_C)
        self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 4)
        self.assertEqual(len(bpy.data.collections[coll_B].all_objects), 1)
        self.assertEqual(len(bpy.data.collections[coll_C].all_objects), 0)

        # Scenario should produce 2 main libraries and 2 archives (empty imports should not
        # result in libraries being created).
        self.assertEqual(len(bpy.data.libraries), 4)
        self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive == False]), 2)
        self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive]), 2)

        # Ensure remove works here as well
        self.remove_collection_importer(coll_A)
        self.assertEqual(len(bpy.data.libraries), 2)
        self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive == False]), 1)
        self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive]), 1)

        self.remove_collection_importer(coll_B)
        self.remove_collection_importer(coll_C)
        self.assertEqual(len(bpy.data.libraries), 0)

    def test_import_multi(self):
        # Validate multiple importers all using the same external file.
        self.reset_blender()

        coll_A = "CollectionA"
        coll_B = "CollectionB"
        coll_C = "CollectionC"
        coll_main = bpy.context.scene.collection
        self.create_collections(coll_main, (coll_A, coll_B, coll_C))

        self.add_collection_importer(coll_A, "IO_FH_usd")
        self.add_collection_importer(coll_B, "IO_FH_usd")
        self.add_collection_importer(coll_C, "IO_FH_usd")

        # Setup each importer using the same external file
        coll = bpy.data.collections[coll_A]
        coll.importer.filepath = str(self.testdir / "import-default.usda")

        coll = bpy.data.collections[coll_B]
        coll.importer.filepath = str(self.testdir / "import-default.usda")
        coll.importer.import_properties.prim_path_mask = "/root/Cube"

        coll = bpy.data.collections[coll_C]
        coll.importer.filepath = str(self.testdir / "import-default.usda")
        coll.importer.import_properties.prim_path_mask = "/root/does_not_exist"

        # Import and validate
        self.do_collection_import(coll_A)
        self.do_collection_import(coll_B)
        self.do_collection_import(coll_C)
        self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 4)
        self.assertEqual(len(bpy.data.collections[coll_B].all_objects), 1)
        self.assertEqual(len(bpy.data.collections[coll_C].all_objects), 0)

        # Scenario should produce 1 main library and 2 archives
        self.assertEqual(len(bpy.data.libraries), 3)
        self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive == False]), 1)
        self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive]), 2)

    def test_link_after_import(self):
        # Validate that a remote collection, which has an importer, is able to
        # be linked in with all its contents.
        self.reset_blender()

        coll_A = "CollectionA"
        coll_main = bpy.context.scene.collection
        self.create_collections(coll_main, (coll_A, ))

        remote_blend = self.tempdir / "remote.blend"

        # Add a new importer and perform the import
        self.add_collection_importer(coll_A, "IO_FH_usd")

        coll = bpy.data.collections[coll_A]
        coll.importer.filepath = str(self.testdir / "import-default.usda")

        self.do_collection_import(coll_A)
        self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 4)

        # Save the current file and reset back to an empty state
        bpy.ops.wm.save_mainfile(filepath=str(remote_blend), check_existing=False, compress=True)

        self.reset_blender()
        self.assertIsNone(bpy.data.collections.get(coll_A))

        # Link in the remote collection and validate all the objects are still in place
        with bpy.data.libraries.load(filepath=str(remote_blend), link=True) as (data_from, data_to):
            data_to.collections.append(coll_A)

        self.assertIsNotNone(bpy.data.collections.get(coll_A))
        # Note: There should be 4 objects but linking is currently disabled
        self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 0)

    def test_reload(self):
        # Validate reload operator.
        self.reset_blender()

        coll_A = "CollectionA"
        coll_B = "CollectionB"
        coll_main = bpy.context.scene.collection
        self.create_collections(coll_main, (coll_A, coll_B))

        self.add_collection_importer(coll_A, "IO_FH_usd")
        self.add_collection_importer(coll_B, "IO_FH_usd")

        # Setup each importer using a unique external file
        self.copy_file(self.testdir / "import-default.usda", self.tempdir / "file1.usda")
        self.copy_file(self.testdir / "import-default.usda", self.tempdir / "file2.usda")

        coll = bpy.data.collections[coll_A]
        coll.importer.filepath = str(self.tempdir / "file1.usda")

        coll = bpy.data.collections[coll_B]
        coll.importer.filepath = str(self.tempdir / "file2.usda")
        coll.importer.import_properties.prim_path_mask = "/root/Cube"

        # Initial state where both collections have imported data
        self.do_collection_import(coll_A)
        self.do_collection_import(coll_B)
        self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 4)
        self.assertEqual(len(bpy.data.collections[coll_B].all_objects), 1)

        # Reload one
        self.reload_collection_importer(coll_A)
        self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 4)
        self.assertEqual(len(bpy.data.collections[coll_B].all_objects), 1)

        # Clear one and reload the other
        self.clear_collection_importer(coll_A)
        self.reload_collection_importer(coll_B)
        self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 0)
        self.assertEqual(len(bpy.data.collections[coll_B].all_objects), 1)

        # Calling reload on an empty collection should still perform an import
        self.reload_collection_importer(coll_A)
        self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 4)
        self.assertEqual(len(bpy.data.collections[coll_B].all_objects), 1)

    def test_clear(self):
        # Validate clear operator.
        self.reset_blender()

        coll_A = "CollectionA"
        coll_B = "CollectionB"
        coll_main = bpy.context.scene.collection
        self.create_collections(coll_main, (coll_A, coll_B))

        self.add_collection_importer(coll_A, "IO_FH_usd")
        self.add_collection_importer(coll_B, "IO_FH_usd")

        # Setup each importer using a unique external file
        self.copy_file(self.testdir / "import-default.usda", self.tempdir / "file1.usda")
        self.copy_file(self.testdir / "import-default.usda", self.tempdir / "file2.usda")

        coll = bpy.data.collections[coll_A]
        coll.importer.filepath = str(self.tempdir / "file1.usda")

        coll = bpy.data.collections[coll_B]
        coll.importer.filepath = str(self.tempdir / "file2.usda")
        coll.importer.import_properties.prim_path_mask = "/root/Cube"

        # Import and clear (twice to ensure state is correctly cleaned up each iteration)
        for _ in range(0, 2):
            self.do_collection_import(coll_A)
            self.do_collection_import(coll_B)
            self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 4)
            self.assertEqual(len(bpy.data.collections[coll_B].all_objects), 1)

            self.assertEqual(len(bpy.data.libraries), 4)
            self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive == False]), 2)
            self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive]), 2)

            self.clear_collection_importer(coll_A)
            self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 0)
            self.assertEqual(len(bpy.data.collections[coll_B].all_objects), 1)

            self.assertEqual(len(bpy.data.libraries), 2)
            self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive == False]), 1)
            self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive]), 1)

            self.clear_collection_importer(coll_B)
            self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 0)
            self.assertEqual(len(bpy.data.collections[coll_B].all_objects), 0)

            self.assertEqual(len(bpy.data.libraries), 0)
            self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive == False]), 0)
            self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive]), 0)

        # No errors should happen when clearing an already empty collection
        self.clear_collection_importer(coll_A)
        self.clear_collection_importer(coll_B)

    def test_make_local(self):
        # Validate make_local operator.
        self.reset_blender()

        coll_A = "CollectionA"
        coll_B = "CollectionB"
        coll_main = bpy.context.scene.collection
        self.create_collections(coll_main, (coll_A, coll_B))

        self.add_collection_importer(coll_A, "IO_FH_usd")
        self.add_collection_importer(coll_B, "IO_FH_usd")

        # Setup each importer using the same external file
        coll = bpy.data.collections[coll_A]
        coll.importer.filepath = str(self.testdir / "import-default.usda")

        coll = bpy.data.collections[coll_B]
        coll.importer.filepath = str(self.testdir / "import-default.usda")
        coll.importer.import_properties.prim_path_mask = "/root/Cube"

        # Import and make local each individually
        self.do_collection_import(coll_A)
        self.do_collection_import(coll_B)
        self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 4)
        self.assertEqual(len(bpy.data.collections[coll_B].all_objects), 1)

        self.assertEqual(len(bpy.data.libraries), 3)
        self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive == False]), 1)
        self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive]), 2)

        # Make the first collection local
        self.make_local_collection(coll_A)
        self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 4)
        self.assertEqual(len(bpy.data.collections[coll_B].all_objects), 1)

        self.assertEqual(len(bpy.data.libraries), 2)
        self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive == False]), 1)
        self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive]), 1)

        # Make the second collection local
        self.make_local_collection(coll_B)
        self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 4)
        self.assertEqual(len(bpy.data.collections[coll_B].all_objects), 1)

        self.assertEqual(len(bpy.data.libraries), 0)


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining, verbosity=0)


if __name__ == "__main__":
    main()
