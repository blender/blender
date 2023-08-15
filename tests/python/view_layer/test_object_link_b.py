# SPDX-FileCopyrightText: 2017-2022 Blender Authors
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
    def test_object_link_context(self):
        """
        See if we can link objects via bpy.context.scene_collection
        """
        import bpy
        bpy.context.window.view_layer = bpy.context.scene.view_layers['Viewport']
        master_collection = bpy.context.scene_collection
        self.do_object_link(master_collection)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
