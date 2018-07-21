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
    def do_syncing(self, filepath_json, unlink_mode):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')

            # open file
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
            self.rename_collections()

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_c = bpy.data.objects.get('T.3c')
            three_d = bpy.data.objects.get('T.3d')

            scene = bpy.context.scene

            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = scene.master_collection.collections['1'].collections.new('scorpion')

            # test linking sync
            subzero.objects.link(three_b)
            scorpion.objects.link(three_c)

            # test unlinking sync
            if unlink_mode in {'OBJECT', 'COLLECTION'}:
                scorpion.objects.link(three_d)
                scorpion.objects.unlink(three_d)

            if unlink_mode == 'COLLECTION':
                scorpion.objects.link(three_d)
                scene.master_collection.collections['1'].collections.remove(subzero)
                scene.master_collection.collections['1'].collections.remove(scorpion)

            # save file
            filepath_nested = os.path.join(dirpath, 'nested.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_nested)

            # get the generated json
            datas = query_scene(filepath_nested, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(datas, "Data is not valid")

            filepath_nested_json = os.path.join(dirpath, "nested.json")
            with open(filepath_nested_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_nested_json,
                filepath_json,
            ),
                "Scene dump files differ")

    def test_syncing_link(self):
        """
        See if scene collections and layer collections are in sync
        when we create new subcollections and link new objects
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_nested.json')
        self.do_syncing(filepath_json, 'NONE')

    def test_syncing_unlink_object(self):
        """
        See if scene collections and layer collections are in sync
        when we create new subcollections, link new objects and unlink
        some.
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_nested.json')
        self.do_syncing(filepath_json, 'OBJECT')

    def test_syncing_unlink_collection(self):
        """
        See if scene collections and layer collections are in sync
        when we create new subcollections, link new objects and unlink full collections
        some.
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers.json')
        self.do_syncing(filepath_json, 'COLLECTION')


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
