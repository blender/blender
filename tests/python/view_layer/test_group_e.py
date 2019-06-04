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
    def test_group_delete_object(self):
        """
        See if we can safely remove instanced objects
        """
        import bpy
        scene = bpy.context.scene
        view_layer = bpy.context.view_layer
        ob = bpy.context.object

        # clean up the scene a bit
        for o in (o for o in view_layer.objects if o != ob):
            view_layer.collections[0].collection.objects.unlink(o)

        for v in (v for v in scene.view_layers if v != view_layer):
            scene.view_layers.remove(v)

        # update depsgraph
        view_layer.update()

        # create group
        group = bpy.data.groups.new("Switch")
        group.objects.link(ob)

        # update depsgraph
        view_layer.update()

        # instance the group
        empty = bpy.data.objects.new("Empty", None)
        bpy.context.scene_collection.objects.link(empty)
        layer_collection = bpy.context.layer_collection
        empty.instance_type = 'GROUP'
        empty.instance_collection = group

        # prepare to delete the original object
        # we could just pass an overridden context
        # but let's do it the old fashion way
        view_layer.objects.active = ob
        ob.select_set(True)
        self.assertTrue(ob.select_get())
        empty.select_set(False)
        self.assertFalse(empty.select_get())

        # update depsgraph
        view_layer.update()

        # delete the original object
        bpy.ops.object.delete()


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
