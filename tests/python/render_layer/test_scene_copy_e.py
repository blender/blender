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
    def test_shared_layer_collections_copy_full(self):
        """
        See if scene copying 'FULL_COPY' is working for scene collections
        with a shared object
        """
        import os
        import bpy

        scene = bpy.context.scene
        layer = bpy.context.render_layer

        original_cube = layer.objects.get('Cube')
        original_cube.select_set('SELECT')
        self.assertTrue(original_cube.select_get())

        bpy.ops.scene.new(type='FULL_COPY')
        new_layer = bpy.context.render_layer

        self.assertNotEqual(layer, new_layer)
        new_cube = new_layer.objects.get('Cube.001')
        self.assertNotEqual(original_cube, new_cube)
        self.assertTrue(new_cube.select_get())


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
