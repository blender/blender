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
        See if the creation of new groups is not crashing anything.
        """
        import bpy
        scene = bpy.context.scene
        layer_collection = bpy.context.layer_collection

        # Cleanup Viewport view layer
        # technically this shouldn't be needed but
        # for now we need it because depsgraph build all the view layers
        # at once.

        while len(scene.view_layers) > 1:
            scene.view_layers.remove(scene.view_layers[1])

        # create group
        group = layer_collection.create_group()

        # update depsgraph
        bpy.context.view_layer.update()


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
