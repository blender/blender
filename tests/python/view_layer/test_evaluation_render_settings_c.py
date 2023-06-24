# SPDX-FileCopyrightText: 2017-2023 Blender Foundation
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
    @unittest.skip("Uses the clay engine, that is removed #55454")
    def test_render_settings(self):
        """
        See if the depsgraph evaluation is correct
        """
        clay = Clay()
        self.assertEqual(clay.get('object', 'matcap_icon'), '01')
        clay.set('mom', 'matcap_icon', '02')
        self.assertEqual(clay.get('object', 'matcap_icon'), '02')


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()
