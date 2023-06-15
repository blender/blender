# SPDX-FileCopyrightText: 2020-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# To run all tests, use
# BLENDER_VERBOSE=1 blender path/to/bevel_regression.blend --python path/to/bevel_operator.py -- --run-all-tests
# To run one test, use
# BLENDER_VERBOSE=1 blender path/to/bevel_regression.blend --python path/to/bevel_operator.py -- --run-test <index>
# where <index> is the index of the test specified in the list tests.

import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import SpecMeshTest, OperatorSpecEditMode, RunTest


def main():
    tests = [
        # 0
        SpecMeshTest('Cube_test_1', 'Cube_test', 'Cube_result_1',

                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2}, 'EDGE', {10})]),
        SpecMeshTest('Cube_test_2', 'Cube_test', 'Cube_result_2',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'offset_type': 'WIDTH'}, 'EDGE', {10, 7}, )]),
        SpecMeshTest('Cube_test_3', 'Cube_test', 'Cube_result_3',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'offset_type': 'DEPTH'}, 'EDGE', {8, 10, 7}, )]),
        SpecMeshTest('Cube_test_4', 'Cube_test', 'Cube_result_4',
                     [OperatorSpecEditMode('bevel', {'offset': 0.4, 'segments': 2}, 'EDGE', {10}, )]),
        SpecMeshTest('Cube_test_5', 'Cube_test', 'Cube_result_5',
                     [OperatorSpecEditMode('bevel', {'offset': 0.4, 'segments': 3}, 'EDGE', {10, 7}, )]),
        # 5
        SpecMeshTest('Cube_test_6', 'Cube_test', 'Cube_result_6',
                     [OperatorSpecEditMode('bevel', {'offset': 0.4, 'segments': 4}, 'EDGE', {8, 10, 7}, )]),
        SpecMeshTest('Cube_test_7', 'Cube_test', 'Cube_result_7',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.4, 'segments': 5, 'profile': 0.2}, 'EDGE', {0, 10, 4, 7}, )]),
        SpecMeshTest('Cube_test_8', 'Cube_test', 'Cube_result_8',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.4, 'segments': 5, 'profile': 0.25}, 'EDGE', {8, 10, 7}, )]),
        SpecMeshTest('Cube_test_9', 'Cube_test', 'Cube_result_9',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.4, 'segments': 6, 'profile': 0.9}, 'EDGE', {8, 10, 7}, )]),
        SpecMeshTest('Cube_test_10', 'Cube_test', 'Cube_result_10',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.4, 'segments': 4, 'profile': 1.0}, 'EDGE', {10, 7}, )]),
        # 10
        SpecMeshTest('Cube_test_11', 'Cube_test', 'Cube_result_11',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.4, 'segments': 5, 'profile': 1.0}, 'EDGE', {8, 10, 7}, )]),
        SpecMeshTest("test 12", 'Cube_test', 'Cube_result_12',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.4, 'segments': 8}, 'EDGE',
                                           {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, )]),
        SpecMeshTest('Pyramid4_test_1', 'Pyr4_test', 'Pyr4_result_1',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {5}, )]),
        SpecMeshTest('Pyramid4_test_2', 'Pyr4_test', 'Pyr4_result_2',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {2, 5}, )]),
        SpecMeshTest('Pyramid4_test_3', 'Pyr4_test', 'Pyr4_result_3',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {2, 3, 5}, )]),
        # 15
        SpecMeshTest('Pyramid4_test_4', 'Pyr4_test', 'Pyr4_result_4',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {1, 2, 3, 5}, )]),
        SpecMeshTest('Pyramid4_test_5', 'Pyr4_test', 'Pyr4_result_5',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 3}, 'EDGE', {1, 2, 3, 5}, )]),
        SpecMeshTest('Pyramid4_test_6', 'Pyr4_test', 'Pyr4_result_6',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 2}, 'EDGE', {2, 3}, )]),
        SpecMeshTest('Pyramid4_test_7', 'Pyr4_test', 'Pyr4_result_7',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 4, 'profile': 0.15}, 'EDGE', {1, 2, 3, 5}, )]),
        SpecMeshTest('Pyramid4_test_8', 'Pyr4_test', 'Pyr4_result_8',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.75, 'segments': 4, 'affect': 'VERTICES'}, 'VERT', {1}, )]),
        # 20
        SpecMeshTest('Pyramid4_test_9', 'Pyr4_test', 'Pyr4_result_9',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.75, 'segments': 3, 'affect': 'VERTICES', 'profile': 0.25}, 'VERT',
                                           {1}, )]),
        SpecMeshTest('Pyramid6_test_1', 'Pyr6_test', 'Pyr6_result_1',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {2, 3}, )]),
        SpecMeshTest('Pyramid6_test_2', 'Pyr6_test', 'Pyr6_result_2',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 2}, 'EDGE', {8, 2, 3}, )]),
        SpecMeshTest('Pyramid6_test_3', 'Pyr6_test', 'Pyr6_result_3',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 4, 'profile': 0.8}, 'EDGE',
                                           {0, 2, 3, 4, 6, 7, 9, 10, 11}, )]),
        SpecMeshTest('Sept_test_1', 'Sept_test', 'Sept_result_1',
                     [OperatorSpecEditMode('bevel', {'offset': 0.1}, 'EDGE', {8, 9, 3, 11}, )]),
        # 25
        SpecMeshTest('Sept_test_2', 'Sept_test', 'Sept_result_2',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.1, 'offset_type': 'WIDTH'}, 'EDGE', {8, 9, 11}, )]),
        SpecMeshTest('Saddle_test_1', 'Saddle_test', 'Saddle_result_1',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.3, 'segments': 5}, 'EDGE', {2, 8, 9, 12, 13, 14}, )]),
        SpecMeshTest('Saddle_test_2', 'Saddle_test', 'Saddle_result_2',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.6, 'segments': 6, 'affect': 'VERTICES'}, 'VERT', {4}, )]),

        SpecMeshTest('Bent_test', 'Bent_test', 'Bent_result_1',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 3},
                                           'EDGE',
                                           {2, 5, 8, 11, 14, 18, 21, 24, 27, 30, 34, 37, 40, 43, 46, 50, 53, 56, 59, 62,
                                            112, 113, 114, 115}, )]),
        SpecMeshTest('Bentlines_test_1', 'Bentlines_test', 'Bentlines_result_1',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 3}, 'EDGE', {1, 8, 9, 10, 11}, )]),
        # 30
        SpecMeshTest('Flaretop_test_1', 'Flaretop_test', 'Flaretop_result_1',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.4, 'segments': 2}, 'EDGE', {26, 12, 20}, )]),
        SpecMeshTest('Flaretop_test_2', 'Flaretop_test', 'Flaretop_result_2',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.4, 'segments': 2, 'profile': 1.0}, 'EDGE', {26, 12, 20}, )]),
        SpecMeshTest('Flaretop_test_3', 'Flaretop_test', 'Flaretop_result_3',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.4, 'segments': 4}, 'FACE', {1, 6, 7, 8, 9, 10, 11, 12}, )]),
        SpecMeshTest('BentL_test', 'BentL_test', 'BentL_result_1',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {4, 8, 10, 18, 24}, )]),
        SpecMeshTest('Wires_test_1', 'Wires_test', 'Wires_test_result_1',
                     [OperatorSpecEditMode('bevel', {'offset': 0.3}, 'EDGE', {0, 1, 2, 10}, )]),
        # 35
        SpecMeshTest('Wires_test_2', 'Wires_test', 'Wires_test_result_2',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.3, 'affect': 'VERTICES'}, 'VERT',
                                           {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, )]),
        SpecMeshTest('tri_test_1', 'tri', 'tri_result_1',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {3, 4, 5}, )]),
        SpecMeshTest('tri_test_2', 'tri', 'tri_result_2',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 2}, 'EDGE', {3, 4, 5}, )]),
        SpecMeshTest('tri_test_3', 'tri', 'tri_result_3',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 3}, 'EDGE', {3, 4, 5}, )]),
        SpecMeshTest('tri_test_4', 'tri', 'tri_result_4',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {3, 4}, )]),
        # 40
        SpecMeshTest('tri_test_5', 'tri', 'tri_result_5',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 2}, 'EDGE', {3, 4}, )]),
        SpecMeshTest('tri_test_6', 'tri', 'tri_result_6',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'affect': 'VERTICES'}, 'VERT', {3}, )]),
        SpecMeshTest('tri_test_7', 'tri', 'tri_result_7',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 2, 'affect': 'VERTICES'}, 'VERT', {3}, )]),
        SpecMeshTest('tri_test_8', 'tri', 'tri_result_8',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 3, 'affect': 'VERTICES'}, 'VERT', {3}, )]),
        SpecMeshTest('tri_test_9', 'tri', 'tri_result_9',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'affect': 'VERTICES'}, 'VERT', {1}, )]),
        # 45
        SpecMeshTest('tri1gap_test_2', 'tri1gap', 'tri1gap_result_2',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 2}, 'EDGE', {3, 4, 5}, )]),
        SpecMeshTest('tri1gap_test_3', 'tri1gap', 'tri1gap_result_3',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 3}, 'EDGE', {3, 4, 5}, )]),
        SpecMeshTest('tri1gap_test_1', 'tri1gap', 'tri1gap_result_1',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {3, 4, 5}, )]),
        SpecMeshTest('tri1gap_test_4', 'tri1gap', 'tri1gap_result_4',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {3, 4}, )]),
        SpecMeshTest('tri1gap_test_5', 'tri1gap', 'tri1gap_result_5',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 2}, 'EDGE', {3, 4}, )]),
        # 50
        SpecMeshTest('tri1gap_test_6', 'tri1gap', 'tri1gap_result_6',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 3}, 'EDGE', {3, 4}, )]),
        SpecMeshTest('tri1gap_test_7', 'tri1gap', 'tri1gap_result_7',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {3, 5}, )]),
        SpecMeshTest('tri1gap_test_8', 'tri1gap', 'tri1gap_result_8',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 2}, 'EDGE', {3, 5}, )]),
        SpecMeshTest('tri1gap_test_9', 'tri1gap', 'tri1gap_result_9',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 3}, 'EDGE', {3, 5}, )]),
        SpecMeshTest('tri1gap_test_10', 'tri1gap', 'tri1gap_result_10',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'affect': 'VERTICES'}, 'VERT', {3}, )]),
        # 55
        SpecMeshTest('tri2gaps_test_1', 'tri2gaps', 'tri2gaps_result_1',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {3, 4, 5}, )]),
        SpecMeshTest('tri2gaps_test_2', 'tri2gaps', 'tri2gaps_result_2',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 2}, 'EDGE', {3, 4, 5}, )]),
        SpecMeshTest('tri2gaps_test_3', 'tri2gaps', 'tri2gaps_result_3',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 3}, 'EDGE', {3, 4, 5}, )]),
        SpecMeshTest('tri2gaps_test_4', 'tri2gaps', 'tri2gaps_result_4',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {3, 4}, )]),
        SpecMeshTest('tri2gaps_test_5', 'tri2gaps', 'tri2gaps_result_5',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 2}, 'EDGE', {3, 4}, )]),
        # 60
        SpecMeshTest('tri2gaps_test_6', 'tri2gaps', 'tri2gaps_result_6',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 3}, 'EDGE', {3, 4}, )]),
        SpecMeshTest('tri3gaps_test_1', 'tri3gaps', 'tri3gaps_result_1',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {3, 4, 5}, )]),
        SpecMeshTest('tri3gaps_test_2', 'tri3gaps', 'tri3gaps_result_2',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 2}, 'EDGE', {3, 4, 5}, )]),
        SpecMeshTest('tri3gaps_test_3', 'tri3gaps', 'tri3gaps_result_3',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 3}, 'EDGE', {3, 4, 5}, )]),
        SpecMeshTest('cube3_test_1', 'cube3', 'cube3_result_1',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2}, 'EDGE', {32, 33, 34, 35, 24, 25, 26, 27, 28, 29, 30, 31}, )]),
        # 65
        SpecMeshTest('cube3_test_2', 'cube3', 'cube3_result_2',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 2}, 'EDGE',
                                           {32, 33, 34, 35, 24, 25, 26, 27, 28, 29, 30, 31}, )]),
        SpecMeshTest('cube3_test_3', 'cube3', 'cube3_result_3',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {32, 35}, )]),
        SpecMeshTest('cube3_test_4', 'cube3', 'cube3_result_4',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2}, 'EDGE', {24, 35}, )]),
        SpecMeshTest('cube3_test_5', 'cube3', 'cube3_result_5',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 2}, 'EDGE', {24, 32, 35}, )]),
        SpecMeshTest('cube3_test_6', 'cube3', 'cube3_result_6',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 3}, 'EDGE', {24, 32, 35}, )]),
        # 70
        SpecMeshTest('Tray', 'Tray', 'Tray_result_1',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.01, 'segments': 2}, 'EDGE', {0, 1, 6, 7, 12, 14, 16, 17}, )]),
        SpecMeshTest("test 73", 'Bumptop', 'Bumptop_result_1',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.1, 'segments': 4}, 'EDGE',
                                           {33, 4, 38, 8, 41, 10, 42, 12, 14, 17, 24, 31}, )]),
        SpecMeshTest('Multisegment_test_1', 'Multisegment_test', 'Multisegment_result_1',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2}, 'EDGE', {16, 14, 15}, )]),
        SpecMeshTest('Window_test', 'Window_test', 'Window_result_1',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.05, 'segments': 2}, 'EDGE', {19, 20, 23, 15}, )]),
        # 75
        SpecMeshTest("test 77", 'Cube_hn_test', 'Cube_hn_result_1',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'harden_normals': True}, 'EDGE', {8}, )]),
        SpecMeshTest('Blocksteps_test_1', 'Blocksteps_test', 'Blocksteps_result_1',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'miter_outer': 'PATCH'}, 'EDGE', {4, 7, 39, 27, 30, 31}, )]),
        SpecMeshTest('Blocksteps_test_2', 'Blocksteps_test', 'Blocksteps_result_2',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 2, 'miter_outer': 'PATCH'}, 'EDGE',
                                           {4, 7, 39, 27, 30, 31}, )]),
        SpecMeshTest('Blocksteps_test_3', 'Blocksteps_test', 'Blocksteps_result_3',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 3, 'miter_outer': 'PATCH'}, 'EDGE',
                                           {4, 7, 39, 27, 30, 31}, )]),
        SpecMeshTest('Blocksteps_test_4', 'Blocksteps_test', 'Blocksteps_result_4',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'miter_outer': 'ARC'}, 'EDGE', {4, 7, 39, 27, 30, 31}, )]),
        # 80
        SpecMeshTest('Blocksteps_test_5', 'Blocksteps_test', 'Blocksteps_result_5',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 2, 'miter_outer': 'ARC'}, 'EDGE',
                                           {4, 7, 39, 27, 30, 31}, )]),
        SpecMeshTest('Blocksteps_test_6', 'Blocksteps_test', 'Blocksteps_result_6',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 3, 'miter_outer': 'ARC'}, 'EDGE',
                                           {4, 7, 39, 27, 30, 31}, )]),
        SpecMeshTest('Blocksteps_test_7', 'Blocksteps_test', 'Blocksteps_result_7',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'miter_outer': 'PATCH', 'miter_inner': 'ARC'}, 'EDGE',
                                           {4, 7, 39, 27, 30, 31}, )]),
        SpecMeshTest("Blocksteps_test_8", 'Blocksteps_test', 'Blocksteps_result_8',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 2, 'miter_outer': 'PATCH', 'miter_inner': 'ARC'},
                                           'EDGE', {4, 7, 39, 27, 30, 31}, )]),
        SpecMeshTest('Blocksteps2_test', 'Blocksteps2_test', 'Blocksteps2_result_9',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 2, 'miter_outer': 'ARC'}, 'EDGE',
                                           {4, 7, 39, 27, 30, 31}, )]),
        # 85
        SpecMeshTest('Blocksteps3_test', 'Blocksteps3_test', 'Blocksteps3_result_10',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 2, 'miter_outer': 'ARC'}, 'EDGE',
                                           {4, 7, 39, 27, 30, 31}, )]),
        SpecMeshTest('Blocksteps4_test_1', 'Blocksteps4_test', 'Blocksteps4_result_11',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 2, 'miter_outer': 'ARC'}, 'EDGE',
                                           {4, 7, 39, 27, 30, 31}, )]),
        SpecMeshTest('Blocksteps4_test_2', 'Blocksteps4_test', 'Blocksteps4_result_12',
                     [OperatorSpecEditMode('bevel',
                                           {'offset': 0.2, 'segments': 3, 'miter_outer': 'ARC'}, 'EDGE',
                                           {4, 7, 39, 27, 30, 31}, )]),
        SpecMeshTest('Spike_test', 'Spike_test', 'Spike_result_1',
                     [OperatorSpecEditMode('bevel', {'offset': 0.2, 'segments': 3}, 'EDGE', {1, 7})])

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
