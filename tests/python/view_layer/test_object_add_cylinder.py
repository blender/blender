# SPDX-FileCopyrightText: 2017-2022 Blender Foundation
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
    def test_syncing_object_add_cylinder(self):
        """
        See if new objects are added to the correct collection
        bpy.ops.mesh.primitive_cylinder_add()
        """
        import os
        ROOT = self.get_root()
        filepath_json = os.path.join(ROOT, 'layers_object_add_cylinder.json')
        self.do_object_add(filepath_json, 'CYLINDER')


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
