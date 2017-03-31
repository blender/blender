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
    def test_visibility(self):
        """
        See if we can link objects
        """
        import bpy

        scene = bpy.context.scene
        cube = bpy.data.objects.new('guinea pig', bpy.data.meshes.new('mesh'))

        layer = scene.render_layers.new('Visibility Test')
        layer.collections.unlink(layer.collections[0])
        scene.render_layers.active = layer

        scene_collection_a = scene.master_collection.collections.new("Visible")
        scene_collection_b = scene.master_collection.collections.new("Invisible")

        scene_collection_a.objects.link(cube)
        scene_collection_b.objects.link(cube)

        layer_collection_a = layer.collections.link(scene_collection_a)
        layer_collection_b = layer.collections.link(scene_collection_b)

        layer_collection_a.hide = False
        layer_collection_b.hide = True

        self.assertTrue(cube.visible_get(), "Object is not visible")


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
