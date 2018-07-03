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
    def test_active_collection(self):
        """
        See if active collection index is working
        layer.collections.active_index works recursively
        """
        import bpy
        import os

        ROOT = self.get_root()
        filepath_layers = os.path.join(ROOT, 'layers.blend')

        # open file
        bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
        self.rename_collections()

        # create sub-collections
        three_b = bpy.data.objects.get('T.3b')
        three_c = bpy.data.objects.get('T.3c')

        scene = bpy.context.scene
        subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
        scorpion = subzero.collections.new('scorpion')
        subzero.objects.link(three_b)
        scorpion.objects.link(three_c)
        layer = scene.view_layers.new('Fresh new Layer')
        layer.collections.link(subzero)

        lookup = [
            'Master Collection',
            '1',
            'sub-zero',
            'scorpion',
            '2',
            '3',
            '4',
            '5',
            'sub-zero',
            'scorpion']

        for i, name in enumerate(lookup):
            layer.collections.active_index = i
            self.assertEqual(
                name, layer.collections.active.name,
                "Collection index mismatch: [{0}] : {1} != {2}".format(
                    i, name, layer.collections.active.name))


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
