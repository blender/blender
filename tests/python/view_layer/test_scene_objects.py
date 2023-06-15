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

class UnitTesting(ViewLayerTesting):
    def test_scene_objects_a(self):
        """
        Test vanilla scene
        """
        import bpy

        scene = bpy.context.scene
        self.assertEqual(len(scene.objects), 3)

    def test_scene_objects_b(self):
        """
        Test scene with nested collections
        """
        import bpy
        scene = bpy.context.scene

        # move default objects to a nested collection
        master_collection = scene.master_collection
        collection = master_collection.collections[0]
        collection_nested = collection.collections.new()

        for ob in collection.objects:
            collection_nested.objects.link(ob)

        while collection.objects:
            collection.objects.unlink(collection.objects[0])

        self.assertEqual(len(scene.objects), 3)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
