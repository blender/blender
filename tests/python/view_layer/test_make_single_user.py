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
    def test_make_single_user(self):
        """
        Really basic test, just to check for crashes on basic files.
        """
        import bpy
        scene = bpy.context.scene
        master_collection = scene.master_collection
        view_layer = bpy.context.view_layer
        ob = bpy.context.object

        # clean up the scene a bit
        for o in (o for o in view_layer.objects if o != ob):
            view_layer.collections[0].collection.objects.unlink(o)

        for v in (v for v in scene.view_layers if v != view_layer):
            scene.view_layers.remove(v)

        while master_collection.collections:
            master_collection.collections.remove(
                master_collection.collections[0])

        view_layer.collections.link(master_collection)
        ob.select_set('SELECT')

        # update depsgraph
        scene.update()

        # test itself
        bpy.ops.object.make_single_user(object=True)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
