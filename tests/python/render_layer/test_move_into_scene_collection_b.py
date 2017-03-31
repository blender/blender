# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

import unittest
import os
import sys

sys.path.append(os.path.dirname(__file__))
from render_layer_common import *


# ############################################################
# Testing
# ############################################################

class UnitTesting(MoveSceneCollectionTesting):
    def get_reference_scene_tree_map(self):
        reference_tree_map = [
                ['A', [
                    ['i', None],
                    ['ii', None],
                    ['iii', None],
                    ]],
                ['B', [
                    ['3', [
                        ['dog', None],
                        ['cat', None],
                        ]],
                    ]],
                ['C', [
                    ['1', None],
                    ['2', None],
                    ]],
                ]
        return reference_tree_map

    def test_scene_collection_into(self):
        """
        Test outliner operations
        """
        tree = self.setup_tree()
        self.assertTrue(tree['3'].move_into(tree['B']))
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
