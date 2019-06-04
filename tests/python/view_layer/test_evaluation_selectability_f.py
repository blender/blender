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
        import bpy
        scene = bpy.context.scene
        view_layer = bpy.context.view_layer

        cube = bpy.data.objects.new('guinea pig', bpy.data.meshes.new('mesh'))
        scene_collection = scene.master_collection.collections.new('collection')
        layer_collection = view_layer.collections.link(scene_collection)

        view_layer.update()  # update depsgraph

        scene_collection.objects.link(cube)

        self.assertTrue(layer_collection.enabled)
        self.assertTrue(layer_collection.selectable)

        view_layer.update()  # update depsgraph
        cube.select_set(True)
        self.assertTrue(cube.select_get())


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
