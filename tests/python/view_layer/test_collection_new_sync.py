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
    def test_view_layer_syncing(self):
        """
        See if we can copy view layers.
        """
        import bpy
        scene = bpy.context.scene
        view_layer = scene.view_layers.new("All")

        self.assertEqual(len(view_layer.collections), 1)
        self.assertEqual(view_layer.collections[0].collection, scene.master_collection)

        self.assertEqual(
            {collection.name for collection in view_layer.collections[0].collections},
            {'Collection 1'})

        self.assertEqual(
            bpy.ops.outliner.collection_new(),
            {'FINISHED'})

        self.assertEqual(
            {collection.name for collection in view_layer.collections[0].collections},
            {'Collection 1', 'Collection 2'})


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
