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

class UnitTesting(MoveSceneCollectionTesting):
    def get_reference_scene_tree_map(self):
        reference_tree_map = [
            ['A', [
                ['i', None],
                ['iii', None],
            ]],
            ['B', None],
            ['C', [
                ['1', None],
                ['2', [
                    ['ii', None],
                ]],
                ['3', [
                    ['dog', None],
                    ['cat', None],
                ]],
            ]],
        ]
        return reference_tree_map

    def test_scene_collection_into(self):
        """
        Test outliner operations
        """
        tree = self.setup_tree()
        self.assertTrue(tree['ii'].move_into(tree['2']))
        self.compare_tree_maps()


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
