# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

from render_layer_common import *
import unittest
import os
import sys

sys.path.append(os.path.dirname(__file__))


# ############################################################
# Testing
# ############################################################

class UnitTesting(RenderLayerTesting):
    def test_object_link_context(self):
        """
        See if we can link objects via bpy.context.scene_collection
        """
        import bpy
        bpy.context.scene.render_layers.active_index = len(bpy.context.scene.render_layers) - 1
        master_collection = bpy.context.scene_collection
        self.do_object_link(master_collection)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
