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
    def test_visibility_nested(self):
        """
        See if the depsgraph evaluation is correct
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
        ob = bpy.data.objects.new('An Empty', None)
        collection_nested.objects.link(ob)

        layer_collection = bpy.context.view_layer.collections.link(master_collection)
        self.assertTrue(layer_collection.enabled)

        # Update depsgraph.
        bpy.context.view_layer.update()

        self.assertTrue(ob.visible_get())

        layer_collection.enabled = False
        self.assertFalse(layer_collection.enabled)

        # Update depsgraph.
        bpy.context.view_layer.update()

        self.assertFalse(ob.visible_get())


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
