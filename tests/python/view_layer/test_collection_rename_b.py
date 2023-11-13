# SPDX-FileCopyrightText: 2018-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

import unittest

from view_layer_common import *


# ############################################################
# Testing
# ############################################################

class UnitTesting(ViewLayerTesting):
    def setup_collections(self):
        import bpy
        scene = bpy.context.scene

        master = scene.master_collection
        one = master.collections[0]
        two = master.collections.new()
        sub = two.collections.new(one.name)

        self.assertEqual(one.name, sub.name)

        lookup = {
            'master': master,
            'one': one,
            'two': two,
            'sub': sub,
        }
        return lookup

    def test_move_above(self):
        collections = self.setup_collections()
        collections['sub'].move_above(collections['one'])
        self.assertNotEqual(collections['one'].name, collections['sub'].name)

    def test_move_below(self):
        collections = self.setup_collections()
        collections['sub'].move_below(collections['one'])
        self.assertNotEqual(collections['one'].name, collections['sub'].name)

    def test_move_into(self):
        collections = self.setup_collections()
        collections['sub'].move_into(collections['master'])
        self.assertNotEqual(collections['one'].name, collections['sub'].name)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
