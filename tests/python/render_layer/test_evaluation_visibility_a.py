# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

import unittest
import os
import sys

from render_layer_common import *


# ############################################################
# Testing
# ############################################################

class UnitTesting(RenderLayerTesting):
    def test_visibility(self):
        """
        See if the depsgraph evaluation is correct
        """
        import bpy

        scene = bpy.context.scene
        workspace = bpy.context.workspace
        cube = bpy.data.objects.new('guinea pig', bpy.data.meshes.new('mesh'))

        layer = scene.render_layers.new('Visibility Test')
        layer.collections.unlink(layer.collections[0])
        workspace.render_layer = layer

        scene_collection_mom = scene.master_collection.collections.new("Mom")
        scene_collection_kid = scene.master_collection.collections.new("Kid")

        scene_collection_mom.objects.link(cube)
        scene_collection_kid.objects.link(cube)

        layer_collection_mom = layer.collections.link(scene_collection_mom)
        layer_collection_kid = layer.collections.link(scene_collection_kid)

        layer_collection_mom.hide = True
        layer_collection_kid.hide = False

        bpy.context.scene.update()  # update depsgraph
        self.assertTrue(cube.visible_get(), "Object should be visible")


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
