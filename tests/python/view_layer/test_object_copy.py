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
    def do_object_copy(self, mode):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')
            filepath_json = os.path.join(ROOT, 'layers_object_copy_duplicate.json')

            # open file
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
            self.rename_collections()

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_c = bpy.data.objects.get('T.3c')

            scene = bpy.context.scene
            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = subzero.collections.new('scorpion')
            subzero.objects.link(three_b)
            scorpion.objects.link(three_c)
            layer = scene.view_layers.new('Fresh new Layer')
            layer.collections.link(subzero)

            bpy.context.window.view_layer = bpy.context.scene.view_layers['Fresh new Layer']

            if mode == 'DUPLICATE':
                # assuming the latest layer is the active layer
                bpy.ops.object.select_all(action='DESELECT')
                three_c.select_set(action='SELECT')
                bpy.ops.object.duplicate()

            elif mode == 'NAMED':
                bpy.ops.object.add_named(name=three_c.name)

            # save file
            filepath_objects = os.path.join(dirpath, 'objects.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_objects)

            # get the generated json
            datas = query_scene(filepath_objects, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(datas, "Data is not valid")

            filepath_objects_json = os.path.join(dirpath, "objects.json")
            with open(filepath_objects_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_objects_json,
                filepath_json,
            ),
                "Scene dump files differ")

    def test_copy_object(self):
        """
        OBJECT_OT_duplicate
        """
        self.do_object_copy('DUPLICATE')

    def test_copy_object_named(self):
        """
        OBJECT_OT_add_named
        """
        self.do_object_copy('NAMED')


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
