# ./blender.bin --background -noaudio --python tests/python/render_layer/test_link.py -- --testdir="/data/lib/tests/"

# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

import unittest

import os, sys
sys.path.append(os.path.dirname(__file__))

from render_layer_common import *


# ############################################################
# Testing
# ############################################################

class UnitTesting(RenderLayerTesting):
    def do_link(self, master_collection):
        import bpy
        self.assertEqual(master_collection.name, "Master Collection")
        self.assertEqual(master_collection, bpy.context.scene.master_collection)
        master_collection.objects.link(bpy.data.objects.new('object', None))

    def test_link_scene(self):
        """
        See if we can link objects
        """
        import bpy
        master_collection = bpy.context.scene.master_collection
        self.do_link(master_collection)

    def test_link_context(self):
        """
        See if we can link objects via bpy.context.scene_collection
        """
        import bpy
        bpy.context.scene.render_layers.active_index = len(bpy.context.scene.render_layers) - 1
        master_collection = bpy.context.scene_collection
        self.do_link(master_collection)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
