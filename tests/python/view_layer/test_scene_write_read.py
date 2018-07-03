# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

import unittest
import os
import sys

from view_layer_common import *


# ############################################################
# Testing
# ############################################################

class UnitTesting(ViewLayerTesting):
    def do_scene_write_read(self, filepath_layers, filepath_layers_json, data_callbacks, do_read):
        """
        See if write/read is working for scene collections and layers
        """
        import bpy
        import os
        import tempfile
        import filecmp

        with tempfile.TemporaryDirectory() as dirpath:
            (self.path_exists(f) for f in (filepath_layers, filepath_layers_json))

            filepath_doversion = os.path.join(dirpath, 'doversion.blend')
            filepath_saved = os.path.join(dirpath, 'doversion_saved.blend')
            filepath_read_json = os.path.join(dirpath, "read.json")

            # doversion + write test
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
            self.rename_collections()
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_doversion)

            datas = query_scene(filepath_doversion, 'Main', data_callbacks)
            self.assertTrue(datas, "Data is not valid")

            filepath_doversion_json = os.path.join(dirpath, "doversion.json")
            with open(filepath_doversion_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_doversion_json,
                filepath_layers_json,
            ),
                "Run: test_scene_write_layers")

            if do_read:
                # read test, simply open and save the file
                bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_doversion)
                self.rename_collections()
                bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_saved)

                datas = query_scene(filepath_saved, 'Main', data_callbacks)
                self.assertTrue(datas, "Data is not valid")

                with open(filepath_read_json, "w") as f:
                    for data in datas:
                        f.write(dump(data))

                self.assertTrue(compare_files(
                    filepath_read_json,
                    filepath_layers_json,
                ),
                    "Scene dump files differ")

    def test_scene_write_collections(self):
        """
        See if the doversion and writing are working for scene collections
        """
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        filepath_layers_json = os.path.join(ROOT, 'layers_simple.json')

        self.do_scene_write_read(
            filepath_layers,
            filepath_layers_json,
            (get_scene_collections,),
            False)

    def test_scene_write_layers(self):
        """
        See if the doversion and writing are working for collections and layers
        """
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        filepath_layers_json = os.path.join(ROOT, 'layers.json')

        self.do_scene_write_read(
            filepath_layers,
            filepath_layers_json,
            (get_scene_collections, get_layers),
            False)

    def test_scene_read_collections(self):
        """
        See if read is working for scene collections
        (run `test_scene_write_colections` first)
        """
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        filepath_layers_json = os.path.join(ROOT, 'layers_simple.json')

        self.do_scene_write_read(
            filepath_layers,
            filepath_layers_json,
            (get_scene_collections,),
            True)

    def test_scene_read_layers(self):
        """
        See if read is working for scene layers
        (run `test_scene_write_layers` first)
        """
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')
        filepath_layers_json = os.path.join(ROOT, 'layers.json')

        self.do_scene_write_read(
            filepath_layers,
            filepath_layers_json,
            (get_scene_collections, get_layers),
            True)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
