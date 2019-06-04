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
    def test_group_create_basic(self):
        """
        More advanced creation of group from a collection not directly linked
        to the scene layer.
        """
        import bpy
        scene = bpy.context.scene

        # clean slate
        self.cleanup_tree()

        children = [bpy.data.objects.new("Child", None) for i in range(3)]
        master_collection = scene.master_collection

        grandma_scene_collection = master_collection.collections.new('Grand-Mother')
        mom_scene_collection = grandma_scene_collection.collections.new('Mother')

        grandma_scene_collection.objects.link(children[0])
        mom_scene_collection.objects.link(children[1])

        grandma_layer_collection = scene.view_layers[0].collections.link(grandma_scene_collection)
        mom_layer_collection = grandma_layer_collection.collections[mom_scene_collection.name]

        # update depsgraph
        bpy.context.view_layer.update()

        # create group
        group = mom_layer_collection.create_group()

        # update depsgraph
        bpy.context.view_layer.update()


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
