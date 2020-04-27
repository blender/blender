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

# To run all tests, use
# BLENDER_VERBOSE=1 blender path/to/bool_regression.blend --python path/to/boolean_operator.py -- --run-all-tests
# To run one test, use
# BLENDER_VERBOSE=1 blender path/to/bool_regression.blend --python path/to/boolean_operator.py -- --run-test <index>
# where <index> is the index of the test specified in the list tests.

import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import OperatorTest


def main():
    tests = [
        ['FACE', {0, 1, 2, 3, 4, 5}, 'Cubecube', 'Cubecube_result_1', 'intersect_boolean', {'operation': 'UNION'}],
        ['FACE', {0, 1, 2, 3, 4, 5}, 'Cubecube', 'Cubecube_result_2', 'intersect_boolean', {'operation': 'INTERSECT'}],
        ['FACE', {0, 1, 2, 3, 4, 5}, 'Cubecube', 'Cubecube_result_3', 'intersect_boolean', {'operation': 'DIFFERENCE'}],
        ['FACE', {0, 1, 2, 3, 4, 5}, 'Cubecube', 'Cubecube_result_4', 'intersect', {'separate_mode': 'CUT'}],
        ['FACE', {0, 1, 2, 3, 4, 5}, 'Cubecube', 'Cubecube_result_5', 'intersect', {'separate_mode': 'ALL'}],
        ['FACE', {0, 1, 2, 3, 4, 5}, 'Cubecube', 'Cubecube_result_6', 'intersect', {'separate_mode': 'NONE'}],
        ['FACE', {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 'Cubecube', 'Cubecube_result_7', 'intersect',
         {'mode': 'SELECT', 'separate_mode': 'NONE'}],
        ['FACE', {6, 7, 8, 9, 10}, 'Cubecone', 'Cubecone_result_1', 'intersect_boolean', {'operation': 'UNION'}],
        ['FACE', {0, 1, 2, 3, 4, 5}, 'Cubecones', 'Cubecones_result_1', 'intersect_boolean', {'operation': 'UNION'}],
    ]

    operator_test = OperatorTest(tests)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            operator_test.run_all_tests()
            break
        elif cmd == "--run-test":
            index = int(command[i + 1])
            operator_test.run_test(index)
            break


if __name__ == "__main__":
    main()
