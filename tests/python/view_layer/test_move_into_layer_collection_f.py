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

class UnitTesting(MoveLayerCollectionTesting):
    def get_reference_scene_tree_map(self):
        reference_tree_map = [
            ['A', [
                ['i', None],
                ['ii', None],
                ['iii', None],
                ['3', [
                    ['dog', None],
                    ['cat', None],
                ]],
            ]],
            ['B', None],
            ['C', [
                ['1', None],
                ['2', None],
            ]],
        ]
        return reference_tree_map

    def get_reference_layers_tree_map(self):
        reference_layers_map = [
            ['Layer 1', [
                'Master Collection',
                'C',
            ]],
            ['Layer 2', [
                'C',
                '3',
                'dog',
                'cat',
            ]],
        ]
        return reference_layers_map

    def test_layer_collection_into_a(self):
        """
        Test outliner operations
        """
        self.setup_tree()
        self.assertTrue(self.move_into("Layer 1.3", "Layer 1.Master Collection.A"))
        self.compare_tree_maps()

    def test_layer_collection_into_b(self):
        """
        Test outliner operations
        """
        self.setup_tree()

        # collection that will be moved
        collection_original = self.parse_move('Layer 1.3')
        collection_original.enabled = True
        collection_original.selectable = False

        self.assertTrue(self.move_into('Layer 1.3', 'Layer 1.Master Collection.A'))
        self.compare_tree_maps()

        # we expect the settings to be carried along from the
        # original layer collection
        collection_new = self.parse_move('Layer 1.Master Collection.A.3')
        self.assertEqual(collection_new.enabled, True)
        self.assertEqual(collection_new.selectable, False)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
