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

class UnitTesting(MoveLayerCollectionTesting):
    def get_reference_scene_tree_map(self):
        # original tree, no changes
        return self.get_initial_scene_tree_map()

    def get_reference_layers_tree_map(self):
        # original tree, no changes
        return self.get_initial_layers_tree_map()

    def test_layer_collection_move_a(self):
        """
        Test outliner operations
        """
        self.setup_tree()
        self.assertFalse(self.move_below('Layer 1.3', 'Layer 1.C'))
        self.compare_tree_maps()

    def test_layer_collection_move_b(self):
        """
        Test outliner operations
        """
        self.setup_tree()
        self.assertFalse(self.move_above('Layer 1.C', 'Layer 1.3'))
        self.compare_tree_maps()


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
