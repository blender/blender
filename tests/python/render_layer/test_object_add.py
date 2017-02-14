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
    def do_object_add(self, filepath_json, add_mode):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')

            # open file
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_c = bpy.data.objects.get('T.3c')

            scene = bpy.context.scene
            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = subzero.collections.new('scorpion')
            subzero.objects.link(three_b)
            scorpion.objects.link(three_c)
            layer = scene.render_layers.new('Fresh new Layer')
            layer.collections.link(subzero)

            # change active collection
            layer.collections.active_index = 3
            self.assertEqual(layer.collections.active.name, 'scorpion', "Run: test_syncing_object_add")

            # change active layer
            override = bpy.context.copy()
            override["render_layer"] = layer
            override["scene_collection"] = layer.collections.active.collection

            # add new objects
            if add_mode == 'EMPTY':
                bpy.ops.object.add(override) # 'Empty'

            elif add_mode == 'CYLINDER':
                bpy.ops.mesh.primitive_cylinder_add(override) # 'Cylinder'

            elif add_mode == 'TORUS':
                bpy.ops.mesh.primitive_torus_add(override) # 'Torus'

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

    def test_syncing_object_add_empty(self):
        """
        See if new objects are added to the correct collection
        bpy.ops.object.add()
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_object_add_empty.json')
        self.do_object_add(filepath_json, 'EMPTY')

    def test_syncing_object_add_cylinder(self):
        """
        See if new objects are added to the correct collection
        bpy.ops.mesh.primitive_cylinder_add()
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_object_add_cylinder.json')
        self.do_object_add(filepath_json, 'CYLINDER')

    def test_syncing_object_add_torus(self):
        """
        See if new objects are added to the correct collection
        bpy.ops.mesh.primitive_torus_add()
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_object_add_torus.json')
        self.do_object_add(filepath_json, 'TORUS')


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
