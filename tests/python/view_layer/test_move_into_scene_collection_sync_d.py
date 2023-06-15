# SPDX-FileCopyrightText: 2017-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

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
        return self.get_initial_scene_tree_map()

    def test_scene_collection_into(self):
        """
        Test outliner operations
        """
        tree = self.setup_tree()
        # can't move into a collection if already the last item of the collection
        self.assertFalse(tree['cat'].move_into(tree['3']))
        self.compare_tree_maps()


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
