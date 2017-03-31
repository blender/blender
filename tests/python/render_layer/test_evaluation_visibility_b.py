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

        scene_collection_mom = scene.master_collection.collections.new("Visible")
        scene_collection_kid = scene_collection_mom.collections.new("Invisible")

        scene_collection_kid.objects.link(cube)

        layer_collection_mom = layer.collections.link(scene_collection_mom)
        layer_collection_kid = layer.collections.link(scene_collection_kid)

        layer_collection_mom.hide = False
        layer_collection_mom.collections[layer_collection_kid.name].hide = True
        layer_collection_kid.hide = True

        self.assertFalse(cube.visible_get(), "Object is not invisible")


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
