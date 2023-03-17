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
    def test_object_link_reload(self):
        """
        See if we can link objects and not crash
        """
        import bpy
        master_collection = bpy.context.scene.master_collection
        self.do_object_link(master_collection)

        # force depsgraph to update
        bpy.ops.wm.read_factory_settings()


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
