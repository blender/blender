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
    def test_group_write_load(self):
        """
        See if saving/loading is working for groups
        """
        import bpy
        scene = bpy.context.scene
        layer_collection = bpy.context.layer_collection

        while len(scene.view_layers) > 1:
            scene.view_layers.remove(scene.view_layers[1])

        # create group
        group = layer_collection.create_group()

        self.assertEqual(1, len(bpy.data.groups))
        self.assertEqual(1, bpy.data.groups[0].users)
        self.assertEqual(3, len(bpy.data.groups[0].objects))

        import os
        import tempfile
        with tempfile.TemporaryDirectory() as dirpath:
            filepath = os.path.join(dirpath, 'layers.blend')

            for i in range(3):
                # save and re-open file
                bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath)
                bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath)

                self.assertEqual(1, len(bpy.data.groups))
                self.assertEqual(1, bpy.data.groups[0].users)
                self.assertEqual(3, len(bpy.data.groups[0].objects))

            # empty the group of objects
            group = bpy.data.groups[0]
            while group.objects:
                group.view_layer.collections[0].collection.objects.unlink(group.objects[0])

            # save and re-open file
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath)
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath)

            self.assertEqual(1, len(bpy.data.groups))
            self.assertEqual(0, bpy.data.groups[0].users)
            self.assertEqual(0, len(bpy.data.groups[0].objects))

            # save and re-open file
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath)
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath)

            self.assertEqual(0, len(bpy.data.groups))


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
