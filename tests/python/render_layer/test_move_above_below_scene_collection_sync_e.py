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

class UnitTesting(MoveSceneCollectionSyncTesting):
    def get_reference_scene_tree_map(self):
        # original tree, no changes
        reference_tree_map = [
                ['A', [
                    ['i', None],
                    ['ii', None],
                    ]],
                ['B', None],
                ['C', [
                    ['1', None],
                    ['2', None],
                    ['3', [
                        ['dog', None],
                        ['iii', None],
                        ['cat', None],
                        ]],
                    ]],
                ]
        return reference_tree_map

    def test_scene_collection_move_a(self):
        """
        Test outliner operations
        """
        tree = self.setup_tree()
        self.assertTrue(tree['iii'].move_below(tree['dog']))
        self.compare_tree_maps()

    def test_scene_collection_move_b(self):
        """
        Test outliner operations
        """
        tree = self.setup_tree()
        self.assertTrue(tree['iii'].move_above(tree['cat']))
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
