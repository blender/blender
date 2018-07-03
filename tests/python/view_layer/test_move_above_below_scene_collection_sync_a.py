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

class UnitTesting(MoveSceneCollectionSyncTesting):
    def get_reference_scene_tree_map(self):
        # original tree, no changes
        reference_tree_map = [
            ['A', [
                ['i', None],
                ['ii', None],
                ['iii', None],
            ]],
            ['B', None],
            ['C', [
                ['1', None],
                ['2', None],
                ['3', [
                    ['dog', None],
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
        self.assertTrue(tree['cat'].move_above(tree['dog']))
        self.assertTrue(tree['dog'].move_above(tree['cat']))
        self.compare_tree_maps()

    def test_scene_collection_move_b(self):
        """
        Test outliner operations
        """
        tree = self.setup_tree()
        self.assertTrue(tree['dog'].move_below(tree['cat']))
        self.assertTrue(tree['cat'].move_below(tree['dog']))
        self.compare_tree_maps()

    def test_scene_collection_move_c(self):
        """
        Test outliner operations
        """
        tree = self.setup_tree()
        self.assertTrue(tree['dog'].move_below(tree['cat']))
        self.assertTrue(tree['dog'].move_above(tree['cat']))
        self.compare_tree_maps()

    def test_scene_collection_move_d(self):
        """
        Test outliner operations
        """
        tree = self.setup_tree()
        self.assertTrue(tree['cat'].move_above(tree['dog']))
        self.assertTrue(tree['cat'].move_below(tree['dog']))
        self.compare_tree_maps()


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
