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
        See if the creation of new groups is preserving visibility flags
        from the original collections.
        """
        import bpy
        scene = bpy.context.scene

        # clean slate
        self.cleanup_tree()

        master_collection = scene.master_collection
        grandma = master_collection.collections.new('бабушка')
        mom = grandma.collections.new('матушка')

        child = bpy.data.objects.new("Child", None)
        mom.objects.link(child)

        grandma_layer_collection = scene.view_layers[0].collections.link(grandma)
        mom_layer_collection = grandma_layer_collection.collections[0]

        grandma_layer_collection.enabled = True
        grandma_layer_collection.enabled = True
        mom_layer_collection.enabled = False
        mom_layer_collection.selectable = True

        # update depsgraph
        bpy.context.view_layer.update()

        # create group
        group = grandma_layer_collection.create_group()

        # update depsgraph
        bpy.context.view_layer.update()

        # compare
        self.assertEqual(len(group.view_layer.collections), 1)
        grandma_group_layer = group.view_layer.collections[0]

        self.assertTrue(grandma_group_layer.enabled, True)
        self.assertTrue(grandma_group_layer.selectable)

        self.assertEqual(len(grandma_group_layer.collections), 1)
        mom_group_layer = grandma_group_layer.collections[0]

        self.assertFalse(mom_group_layer.enabled)
        self.assertTrue(mom_group_layer.selectable)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
