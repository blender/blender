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
    def test_render_settings(self):
        """
        See if the depsgraph evaluation is correct
        """
        clay = Clay()
        self.assertEqual(clay.get('object', 'matcap_icon'), '01')
        clay.set('kid', 'matcap_icon', '05')
        self.assertEqual(clay.get('object', 'matcap_icon'), '05')


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()
