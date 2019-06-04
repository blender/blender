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
    def test_selectability(self):
        """
        See if the depsgraph evaluation is correct
        """
        import bpy

        scene = bpy.context.scene
        window = bpy.context.window
        cube = bpy.data.objects.new('guinea pig', bpy.data.meshes.new('mesh'))

        layer = scene.view_layers.new('Selectability Test')
        layer.collections.unlink(layer.collections[0])
        window.view_layer = layer

        scene_collection_mom = scene.master_collection.collections.new("Mom")
        scene_collection_kid = scene_collection_mom.collections.new("Kid")

        scene_collection_kid.objects.link(cube)

        layer_collection_mom = layer.collections.link(scene_collection_mom)
        layer_collection_kid = layer.collections.link(scene_collection_kid)

        layer_collection_mom.enabled = False
        bpy.context.view_layer.update()  # update depsgraph
        cube.select_set(True)

        self.assertTrue(cube.visible_get(), "Cube should be visible")
        self.assertTrue(cube.select_get(), "Cube should be selected")


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
