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
    def test_scene_collection_delete(self):
        """
        See if a scene collection can be properly deleted even
        when linked
        """
        import bpy

        # delete all initial objects
        while bpy.data.objects:
            bpy.data.objects.remove(bpy.data.objects[0])

        # delete all initial collections
        scene = bpy.context.scene
        master_collection = scene.master_collection
        while master_collection.collections:
            master_collection.collections.remove(master_collection.collections[0])

        collection_parent = master_collection.collections.new('parent')
        collection_nested = collection_parent.collections.new('child linked')
        bpy.context.view_layer.collections.link(collection_nested)
        master_collection.collections.remove(collection_parent)

        # Update depsgraph.
        bpy.context.view_layer.update()


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
