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

    def add_collection_importer(self, collection_name, importer_type, expect_error=False):
        self.set_active_collection(collection_name)

        try:
            bpy.ops.collection.importer_add(name=importer_type)
            self.assertIsNotNone(
                bpy.data.collections[collection_name].importer,
                f"Failed to add {importer_type} importer on collection {collection_name}")
        except RuntimeError as e:
            self.assertTrue(expect_error, f"Unexpected error occurred: {e}")

    def remove_collection_importer(self, collection_name, expect_error=False):
        self.set_active_collection(collection_name)

        try:
            bpy.ops.collection.importer_remove()
            self.assertIsNone(
                bpy.data.collections[collection_name].importer,
                f"Failed to remove importer on collection {collection_name}")
        except RuntimeError as e:
            self.assertTrue(expect_error, f"Unexpected error occurred: {e}")

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
        self.add_collection_importer(coll_B, "IO_FH_usd", expect_error=True)

        self.remove_collection_importer(coll_A)
        self.remove_collection_importer(coll_B)
        self.remove_collection_importer(coll_B, expect_error=True)

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
        self.assertEqual(len(bpy.data.libraries), 6)
        self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive == False]), 3)
        self.assertEqual(len([l for l in bpy.data.libraries if l.is_archive]), 3)

    # Disabled: Multiple importers all referencing the same external file is
    # currently not supported.
    def __disabled_test_import_multi(self):
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

        # Setup each importer
        # NOTE: Add additional validation for bpy.data.libraries once bug with multiple importers
        # all referencing the same external file is fixed.
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
        # TODO: Library validation once scenario is supported

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
        self.assertEqual(len(bpy.data.collections[coll_A].all_objects), 4)


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
