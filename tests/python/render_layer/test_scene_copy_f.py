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
        See if scene copying 'FULL_COPY' is keeping collections visibility
        and selectability.
        """
        import os
        import bpy

        scene = bpy.context.scene

        hide_lookup = [0, 1, 1, 0]
        hide_lookup_sub = [1, 0, 1]

        hide_select_lookup = [0, 0, 1, 1]
        hide_select_lookup_sub = [1, 0, 1, 0]
        new_collections = []

        # clean everything
        for layer in scene.render_layers:
            while layer.collections:
                layer.collections.unlink(layer.collections[0])

        # create new collections
        for i in range(4):
            collection = scene.master_collection.collections.new(str(i))
            new_collections.append(collection)

            for j in range(3):
                sub_collection = collection.collections.new("{0}:{1}".format(i, j))

        # link to the original scene
        for layer in scene.render_layers:
            for i, collection in enumerate(new_collections):
                layer.collections.link(collection)
                self.assertEqual(layer.collections[-1], layer.collections[i])

                layer.collections[i].hide = hide_lookup[i]
                layer.collections[i].hide_select = hide_select_lookup[i]

                for j, sub_collection in enumerate(layer.collections[i].collections):
                    sub_collection.hide = hide_lookup_sub[j]
                    sub_collection.hide_select = hide_select_lookup_sub[j]

        # copy scene
        bpy.ops.scene.new(type='FULL_COPY')
        new_scene = bpy.context.scene
        self.assertNotEqual(scene, new_scene)

        # update depsgrah
        scene.update()  # update depsgraph

        # compare scenes
        for h, layer in enumerate(scene.render_layers):
            new_layer = new_scene.render_layers[h]

            for i, collection in enumerate(layer.collections):
                new_collection = new_layer.collections[i]
                self.assertEqual(collection.hide, new_collection.hide)
                self.assertEqual(collection.hide_select, new_collection.hide_select)

                for j, sub_collection in enumerate(layer.collections[i].collections):
                    new_sub_collection = new_collection.collections[j]
                    self.assertEqual(sub_collection.hide, new_sub_collection.hide)
                    self.assertEqual(sub_collection.hide_select, new_sub_collection.hide_select)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
