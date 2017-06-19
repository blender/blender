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
    def test_object_link_context(self):
        """
        See if we can link objects via bpy.context.scene_collection
        """
        import bpy
        bpy.context.workspace.render_layer = bpy.context.scene.render_layers['Viewport']
        master_collection = bpy.context.scene_collection
        self.do_object_link(master_collection)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
