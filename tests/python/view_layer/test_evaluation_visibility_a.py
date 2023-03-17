# SPDX-License-Identifier: GPL-2.0-or-later

# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

import unittest

from view_layer_common import *


# ############################################################
# Testing
# ############################################################


class UnitTesting(ViewLayerTesting):
    def test_visibility(self):
        """
        See if the depsgraph evaluation is correct
        """
        import bpy

        scene = bpy.context.scene
        window = bpy.context.window
        cube = bpy.data.objects.new("guinea pig", bpy.data.meshes.new("mesh"))

        layer = scene.view_layers.new("Visibility Test")
        layer.collections.unlink(layer.collections[0])
        window.view_layer = layer

        scene_collection_mom = scene.master_collection.collections.new("Mom")
        scene_collection_kid = scene.master_collection.collections.new("Kid")

        scene_collection_mom.objects.link(cube)
        scene_collection_kid.objects.link(cube)

        layer_collection_mom = layer.collections.link(scene_collection_mom)
        layer_collection_kid = layer.collections.link(scene_collection_kid)

        layer_collection_mom.enabled = False
        layer_collection_kid.enabled = True

        layer.update()  # update depsgraph
        self.assertTrue(cube.visible_get(), "Object should be visible")


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == "__main__":
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
