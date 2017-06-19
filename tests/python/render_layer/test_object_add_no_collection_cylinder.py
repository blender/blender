# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

import unittest
import os
import sys

from render_layer_common import *


# ############################################################
# Testing
# ############################################################

class UnitTesting(RenderLayerTesting):
    def test_object_add_cylinder(self):
        """
        See if new objects are added to the correct collection
        bpy.ops.mesh.primitive_cylinder_add()
        """
        import os
        self.do_object_add_no_collection('CYLINDER')


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
