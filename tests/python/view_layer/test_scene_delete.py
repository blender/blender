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
    def test_scene_delete(self):
        """
        See if a scene can be properly deleted
        """
        import bpy

        scene = bpy.context.scene
        bpy.data.scenes.new('New')
        bpy.data.scenes.remove(scene)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
