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
    def do_layer_linking(self, filepath_json, link_mode):
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

            scene = bpy.context.scene

            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = subzero.collections.new('scorpion')

            # test linking sync
            subzero.objects.link(three_b)
            scorpion.objects.link(three_c)

            # test unlinking sync
            layer = scene.view_layers.new('Fresh new Layer')

            if link_mode in {'COLLECTION_LINK', 'COLLECTION_UNLINK'}:
                layer.collections.link(subzero)

            if link_mode == 'COLLECTION_UNLINK':
                initial_collection = layer.collections['Master Collection']
                layer.collections.unlink(initial_collection)

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

    def test_syncing_layer_new(self):
        """
        See if the creation of new layers is going well
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_new_layer.json')
        self.do_layer_linking(filepath_json, 'LAYER_NEW')

    def test_syncing_layer_collection_link(self):
        """
        See if the creation of new layers is going well
        And linking a new scene collection in the layer works
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_layer_collection_link.json')
        self.do_layer_linking(filepath_json, 'COLLECTION_LINK')

    def test_syncing_layer_collection_unlink(self):
        """
        See if the creation of new layers is going well
        And unlinking the origin scene collection works
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_layer_collection_unlink.json')
        self.do_layer_linking(filepath_json, 'COLLECTION_UNLINK')


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
