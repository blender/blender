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
    def test_object_link_reload(self):
        """
        See if we can link objects and not crash
        """
        import bpy
        master_collection = bpy.context.scene.master_collection
        self.do_object_link(master_collection)

        # force depsgraph to update
        bpy.ops.wm.read_factory_settings()


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
