# SPDX-FileCopyrightText: 2020-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# To run all tests, use
# BLENDER_VERBOSE=1 blender path/to/bool_regression.blend --python path/to/boolean_operator.py -- --run-all-tests
# To run one test, use
# BLENDER_VERBOSE=1 blender path/to/bool_regression.blend --python path/to/boolean_operator.py -- --run-test <index>
# where <index> is the index of the test specified in the list tests.

import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import SpecMeshTest, OperatorSpecEditMode, RunTest


def main():
    tests = [

        SpecMeshTest('Cubecube_intersect_union', 'Cubecube', 'Cubecube_result_1',
                     [OperatorSpecEditMode('intersect_boolean',
                                           {'operation': 'UNION', 'solver': 'FAST'}, 'FACE', {0, 1, 2, 3, 4, 5}, )]),
        SpecMeshTest('Cubecube_intersect_intersect', 'Cubecube', 'Cubecube_result_2',
                     [OperatorSpecEditMode('intersect_boolean', {'operation': 'INTERSECT', 'solver': 'FAST'}, 'FACE', {0, 1, 2, 3, 4, 5}, )]),
        SpecMeshTest('Cubecube_intersect_difference', 'Cubecube', 'Cubecube_result_3',
                     [OperatorSpecEditMode('intersect_boolean', {'operation': 'DIFFERENCE', 'solver': 'FAST'}, 'FACE',
                                           {0, 1, 2, 3, 4, 5}, )]),
        SpecMeshTest('Cubecube_intersect_cut', 'Cubecube', 'Cubecube_result_4', [OperatorSpecEditMode('intersect',
                                                                                                      {'separate_mode': 'CUT', 'solver': 'FAST'}, 'FACE', {0, 1, 2, 3, 4, 5}, )]),
        SpecMeshTest('Cubecube_intersect_all', 'Cubecube', 'Cubecube_result_5',
                     [OperatorSpecEditMode('intersect',
                                           {'separate_mode': 'ALL', 'solver': 'FAST'}, 'FACE', {0, 1, 2, 3, 4, 5}, )]),
        SpecMeshTest('Cubecube_intersect_none', 'Cubecube', 'Cubecube_result_6',
                     [OperatorSpecEditMode('intersect',
                                           {'separate_mode': 'NONE', 'solver': 'FAST'}, 'FACE', {0, 1, 2, 3, 4, 5}, )]),
        SpecMeshTest('Cubecube_intersect_select_none', 'Cubecube',
                     'Cubecube_result_7',
                     [OperatorSpecEditMode('intersect',
                                           {'mode': 'SELECT', 'separate_mode': 'NONE', 'solver': 'FAST'}, 'FACE',
                                           {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, )]),
        SpecMeshTest('Cubecone_intersect_union', 'Cubecone', 'Cubecone_result_1',
                     [OperatorSpecEditMode('intersect_boolean',
                                           {'operation': 'UNION', 'solver': 'FAST'}, 'FACE', {6, 7, 8, 9, 10}, )]),
        SpecMeshTest('Cubecones_intersect_union', 'Cubecones', 'Cubecones_result_1',
                     [OperatorSpecEditMode('intersect_boolean', {'operation': 'UNION', 'solver': 'FAST'}, 'FACE', {0, 1, 2, 3, 4, 5}, )]),

    ]

    operator_test = RunTest(tests)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            operator_test.do_compare = True
            operator_test.run_all_tests()
            break
        elif cmd == "--run-test":
            name = command[i + 1]
            operator_test.do_compare = False
            operator_test.run_test(name)
            break


if __name__ == "__main__":
    main()
