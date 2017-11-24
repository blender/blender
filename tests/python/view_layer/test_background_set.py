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
    def test_background_set(self):
        """
        See if background sets are properly added and removed
        """
        import bpy

        background_scene = bpy.data.scenes[0]
        main_scene = bpy.data.scenes.new('main')
        bpy.context.window.scene = main_scene

        # Update depsgraph.
        main_scene.update()
        background_scene.update()

        # Safety check, there should be no objects in thew newly created scene.
        self.assertEqual(0, len(bpy.context.depsgraph.objects))

        # Now set the background set, and objects relationship.
        main_scene.background_set = background_scene
        background_scene.objects[0].parent = background_scene.objects[1]

        # Update depsgraph.
        main_scene.update()
        background_scene.update()

        # Test if objects were properly added to depsgraph.
        self.assertEqual(3, len(bpy.context.depsgraph.objects))

        # At this point the ideal would be to be able to check if
        # the objects are not selected and their relationship line
        # and origin is not visible.
        #
        # However we can't check this from the current API
        # so we either do image comparison, expand the API
        # (won't work relationship lines) or leave as it is.

        # Test if removing is working fine.
        main_scene.background_set = None

        # Update depsgraph.
        main_scene.update()
        background_scene.update()

        self.assertEqual(0, len(bpy.context.depsgraph.objects))


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
