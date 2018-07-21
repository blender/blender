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
                ['ii', None],
                ['iii', None],
            ]],
            ['C', [
                ['1', None],
                ['2', None],
                ['3', [
                    ['dog', None],
                    ['cat', [
                            ['B', None],
                    ]],
                ]],
            ]],
        ]
        return reference_tree_map

    def test_scene_collection_into(self):
        """
        Test outliner operations
        """
        tree = self.setup_tree()
        self.assertTrue(tree['B'].move_into(tree['cat']))
        self.compare_tree_maps()


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
