# SPDX-FileCopyrightText: 2009-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import BlendFileTest

geo_node_test = BlendFileTest("test_object", "expected_object", threshold=1e-4)
result = geo_node_test.run_test()

# Telling `ctest` about the failed test by raising Exception.
if not result:
    raise Exception("Failed {}".format(geo_node_test.test_name))
