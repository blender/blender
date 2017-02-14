# ./blender.bin --background -noaudio --python tests/python/render_layer/test_scene_copy.py -- --testdir="/data/lib/tests/"

# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

import unittest

import os, sys
sys.path.append(os.path.dirname(__file__))

from render_layer_common import *


# ############################################################
# Testing
# ############################################################

class UnitTesting(RenderLayerTesting):
    def do_object_delete(self, del_mode):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')
            filepath_reference_json = os.path.join(ROOT, 'layers_object_delete.json')

            # open file
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_d = bpy.data.objects.get('T.3d')

            scene = bpy.context.scene

            # mangle the file a bit with some objects linked across collections
            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = subzero.collections.new('scorpion')
            subzero.objects.link(three_d)
            scorpion.objects.link(three_b)
            scorpion.objects.link(three_d)

            # object to delete
            ob = three_d

            # delete object
            if del_mode == 'DATA':
                bpy.data.objects.remove(ob, do_unlink=True)

            elif del_mode == 'OPERATOR':
                bpy.ops.object.select_all(action='DESELECT')
                ob.select_set(action='SELECT')
                bpy.ops.object.delete()

            # save file
            filepath_generated = os.path.join(dirpath, 'generated.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_generated)

            # get the generated json
            datas = query_scene(filepath_generated, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(datas, "Data is not valid")

            filepath_generated_json = os.path.join(dirpath, "generated.json")
            with open(filepath_generated_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_generated_json,
                filepath_reference_json,
                ),
                "Scene dump files differ")

    def test_object_delete_data(self):
        """
        See if objects are removed correctly from all related collections
        bpy.data.objects.remove()
        """
        self.do_object_delete('DATA')

    def test_object_delete_operator(self):
        """
        See if new objects are added to the correct collection
        bpy.ops.object.del()
        """
        self.do_object_delete('OPERATOR')


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
