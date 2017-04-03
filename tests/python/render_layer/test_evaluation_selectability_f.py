# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

import unittest
import os
import sys

sys.path.append(os.path.dirname(__file__))
from render_layer_common import *


# ############################################################
# Testing
# ############################################################

class UnitTesting(RenderLayerTesting):
    def test_selectability(self):
        import bpy
        scene = bpy.context.scene

        cube = bpy.data.objects.new('guinea pig', bpy.data.meshes.new('mesh'))
        scene_collection = scene.master_collection.collections.new('collection')
        layer_collection = scene.render_layers.active.collections.link(scene_collection)

        bpy.context.scene.update()  # update depsgraph

        scene_collection.objects.link(cube)

        self.assertFalse(layer_collection.hide)
        self.assertFalse(layer_collection.hide_select)

        bpy.context.scene.update()  # update depsgraph
        cube.select_set(action='SELECT')
        self.assertTrue(cube.select_get())

# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
