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
    def test_shared_layer_collections_copy_full(self):
        """
        See if scene copying 'FULL_COPY' is working for scene collections
        with a shared object
        """
        import os
        import bpy

        scene = bpy.context.scene
        layer = bpy.context.view_layer

        original_cube = layer.objects.get('Cube')
        original_cube.select_set(True)
        self.assertTrue(original_cube.select_get())

        bpy.ops.scene.new(type='FULL_COPY')
        new_layer = bpy.context.view_layer

        self.assertNotEqual(layer, new_layer)
        new_cube = new_layer.objects.get('Cube.001')
        self.assertNotEqual(original_cube, new_cube)
        self.assertTrue(new_cube.select_get())


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
