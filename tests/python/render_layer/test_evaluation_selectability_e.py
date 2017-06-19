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
    def test_selectability(self):
        """
        See if the depsgraph evaluation is correct
        """
        import bpy

        scene = bpy.context.scene
        workspace = bpy.context.workspace
        cube = bpy.data.objects.new('guinea pig', bpy.data.meshes.new('mesh'))

        layer = scene.render_layers.new('Selectability Test')
        layer.collections.unlink(layer.collections[0])
        workspace.render_layer = layer

        scene_collection_mom = scene.master_collection.collections.new("Mom")
        scene_collection_kid = scene_collection_mom.collections.new("Kid")

        scene_collection_kid.objects.link(cube)

        layer_collection_mom = layer.collections.link(scene_collection_mom)
        layer_collection_kid = layer.collections.link(scene_collection_kid)

        layer_collection_mom.hide = False
        cube.select_set('SELECT')
        layer_collection_mom.collections[layer_collection_kid.name].hide_select = True
        layer_collection_kid.hide = True

        bpy.context.scene.update()  # update depsgraph
        self.assertTrue(cube.visible_get(), "Cube should be visible")
        self.assertFalse(cube.select_get(), "Cube should be unselected")


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
