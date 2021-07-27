# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>


import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import BlendFileTest

geo_node_test = BlendFileTest("test_object", "expected_object")
result = geo_node_test.run_test()

# Telling `ctest` about the failed test by raising Exception.
if result == False:
    raise Exception("Failed {}".format(geo_node_test.test_name))
