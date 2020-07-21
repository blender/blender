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

# To run all tests, use
# BLENDER_VERBOSE=1 blender path/to/bevel_regression.blend --python path/to/bevel_operator.py -- --run-all-tests
# To run one test, use
# BLENDER_VERBOSE=1 blender path/to/bevel_regression.blend --python path/to/bevel_operator.py -- --run-test <index>
# where <index> is the index of the test specified in the list tests.

import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import OperatorTest


def main():
    tests = [
        # 0
        ['EDGE', {10}, 'Cube_test', 'Cube_result_1', 'bevel', {'offset': 0.2}],
        ['EDGE', {10, 7}, 'Cube_test', 'Cube_result_2', 'bevel', {'offset': 0.2, 'offset_type': 'WIDTH'}],
        ['EDGE', {8, 10, 7}, 'Cube_test', 'Cube_result_3', 'bevel', {'offset': 0.2, 'offset_type': 'DEPTH'}],
        ['EDGE', {10}, 'Cube_test', 'Cube_result_4', 'bevel', {'offset': 0.4, 'segments': 2}],
        ['EDGE', {10, 7}, 'Cube_test', 'Cube_result_5', 'bevel', {'offset': 0.4, 'segments': 3}],
        # 5
        ['EDGE', {8, 10, 7}, 'Cube_test', 'Cube_result_6', 'bevel', {'offset': 0.4, 'segments': 4}],
        ['EDGE', {0, 10, 4, 7}, 'Cube_test', 'Cube_result_7', 'bevel', {'offset': 0.4, 'segments': 5, 'profile': 0.2}],
        ['EDGE', {8, 10, 7}, 'Cube_test', 'Cube_result_8', 'bevel', {'offset': 0.4, 'segments': 5, 'profile': 0.25}],
        ['EDGE', {8, 10, 7}, 'Cube_test', 'Cube_result_9', 'bevel', {'offset': 0.4, 'segments': 6, 'profile': 0.9}],
        ['EDGE', {10, 7}, 'Cube_test', 'Cube_result_10', 'bevel', {'offset': 0.4, 'segments': 4, 'profile': 1.0}],
        # 10
        ['EDGE', {8, 10, 7}, 'Cube_test', 'Cube_result_11', 'bevel', {'offset': 0.4, 'segments': 5, 'profile': 1.0}],
        ['EDGE', {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 'Cube_test', 'Cube_result_12', 'bevel',
         {'offset': 0.4, 'segments': 8}],
        ['EDGE', {5}, 'Pyr4_test', 'Pyr4_result_1', 'bevel', {'offset': 0.2}],
        ['EDGE', {2, 5}, 'Pyr4_test', 'Pyr4_result_2', 'bevel', {'offset': 0.2}],
        ['EDGE', {2, 3, 5}, 'Pyr4_test', 'Pyr4_result_3', 'bevel', {'offset': 0.2}],
        # 15
        ['EDGE', {1, 2, 3, 5}, 'Pyr4_test', 'Pyr4_result_4', 'bevel', {'offset': 0.2}],
        ['EDGE', {1, 2, 3, 5}, 'Pyr4_test', 'Pyr4_result_5', 'bevel', {'offset': 0.2, 'segments': 3}],
        ['EDGE', {2, 3}, 'Pyr4_test', 'Pyr4_result_6', 'bevel', {'offset': 0.2, 'segments': 2}],
        ['EDGE', {1, 2, 3, 5}, 'Pyr4_test', 'Pyr4_result_7', 'bevel', {'offset': 0.2, 'segments': 4, 'profile': 0.15}],
        ['VERT', {1}, 'Pyr4_test', 'Pyr4_result_8', 'bevel', {'offset': 0.75, 'segments': 4, 'affect': 'VERTICES'}],
        # 20
        ['VERT', {1}, 'Pyr4_test', 'Pyr4_result_9', 'bevel',
         {'offset': 0.75, 'segments': 3, 'affect': 'VERTICES', 'profile': 0.25}],
        ['EDGE', {2, 3}, 'Pyr6_test', 'Pyr6_result_1', 'bevel', {'offset': 0.2}],
        ['EDGE', {8, 2, 3}, 'Pyr6_test', 'Pyr6_result_2', 'bevel', {'offset': 0.2, 'segments': 2}],
        ['EDGE', {0, 2, 3, 4, 6, 7, 9, 10, 11}, 'Pyr6_test', 'Pyr6_result_3', 'bevel',
         {'offset': 0.2, 'segments': 4, 'profile': 0.8}],
        ['EDGE', {8, 9, 3, 11}, 'Sept_test', 'Sept_result_1', 'bevel', {'offset': 0.1}],
        # 25
        ['EDGE', {8, 9, 11}, 'Sept_test', 'Sept_result_2', 'bevel', {'offset': 0.1, 'offset_type': 'WIDTH'}],
        ['EDGE', {2, 8, 9, 12, 13, 14}, 'Saddle_test', 'Saddle_result_1', 'bevel', {'offset': 0.3, 'segments': 5}],
        ['VERT', {4}, 'Saddle_test', 'Saddle_result_2', 'bevel', {'offset': 0.6, 'segments': 6, 'affect': 'VERTICES'}],
        ['EDGE', {2, 5, 8, 11, 14, 18, 21, 24, 27, 30, 34, 37, 40, 43, 46, 50, 53, 56, 59, 62, 112, 113, 114, 115},
         'Bent_test', 'Bent_result_1', 'bevel', {'offset': 0.2, 'segments': 3}],
        ['EDGE', {1, 8, 9, 10, 11}, 'Bentlines_test', 'Bentlines_result_1', 'bevel', {'offset': 0.2, 'segments': 3}],
        # 30
        ['EDGE', {26, 12, 20}, 'Flaretop_test', 'Flaretop_result_1', 'bevel', {'offset': 0.4, 'segments': 2}],
        ['EDGE', {26, 12, 20}, 'Flaretop_test', 'Flaretop_result_2', 'bevel',
         {'offset': 0.4, 'segments': 2, 'profile': 1.0}],
        ['FACE', {1, 6, 7, 8, 9, 10, 11, 12}, 'Flaretop_test', 'Flaretop_result_3', 'bevel',
         {'offset': 0.4, 'segments': 4}],
        ['EDGE', {4, 8, 10, 18, 24}, 'BentL_test', 'BentL_result_1', 'bevel', {'offset': 0.2}],
        ['EDGE', {0, 1, 2, 10}, 'Wires_test', 'Wires_test_result_1', 'bevel', {'offset': 0.3}],
        # 35
        ['VERT', {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, 'Wires_test', 'Wires_test_result_2', 'bevel',
         {'offset': 0.3, 'affect': 'VERTICES'}],
        ['EDGE', {3, 4, 5}, 'tri', 'tri_result_1', 'bevel', {'offset': 0.2}],
        ['EDGE', {3, 4, 5}, 'tri', 'tri_result_2', 'bevel', {'offset': 0.2, 'segments': 2}],
        ['EDGE', {3, 4, 5}, 'tri', 'tri_result_3', 'bevel', {'offset': 0.2, 'segments': 3}],
        ['EDGE', {3, 4}, 'tri', 'tri_result_4', 'bevel', {'offset': 0.2}],
        # 40
        ['EDGE', {3, 4}, 'tri', 'tri_result_5', 'bevel', {'offset': 0.2, 'segments': 2}],
        ['VERT', {3}, 'tri', 'tri_result_6', 'bevel', {'offset': 0.2, 'affect': 'VERTICES'}],
        ['VERT', {3}, 'tri', 'tri_result_7', 'bevel', {'offset': 0.2, 'segments': 2, 'affect': 'VERTICES'}],
        ['VERT', {3}, 'tri', 'tri_result_8', 'bevel', {'offset': 0.2, 'segments': 3, 'affect': 'VERTICES'}],
        ['VERT', {1}, 'tri', 'tri_result_9', 'bevel', {'offset': 0.2, 'affect': 'VERTICES'}],
        # 45
        ['EDGE', {3, 4, 5}, 'tri1gap', 'tri1gap_result_1', 'bevel', {'offset': 0.2}],
        ['EDGE', {3, 4, 5}, 'tri1gap', 'tri1gap_result_2', 'bevel', {'offset': 0.2, 'segments': 2}],
        ['EDGE', {3, 4, 5}, 'tri1gap', 'tri1gap_result_3', 'bevel', {'offset': 0.2, 'segments': 3}],
        ['EDGE', {3, 4}, 'tri1gap', 'tri1gap_result_4', 'bevel', {'offset': 0.2}],
        ['EDGE', {3, 4}, 'tri1gap', 'tri1gap_result_5', 'bevel', {'offset': 0.2, 'segments': 2}],
        # 50
        ['EDGE', {3, 4}, 'tri1gap', 'tri1gap_result_6', 'bevel', {'offset': 0.2, 'segments': 3}],
        ['EDGE', {3, 5}, 'tri1gap', 'tri1gap_result_7', 'bevel', {'offset': 0.2}],
        ['EDGE', {3, 5}, 'tri1gap', 'tri1gap_result_8', 'bevel', {'offset': 0.2, 'segments': 2}],
        ['EDGE', {3, 5}, 'tri1gap', 'tri1gap_result_9', 'bevel', {'offset': 0.2, 'segments': 3}],
        ['VERT', {3}, 'tri1gap', 'tri1gap_result_10', 'bevel', {'offset': 0.2, 'affect': 'VERTICES'}],
        # 55
        ['EDGE', {3, 4, 5}, 'tri2gaps', 'tri2gaps_result_1', 'bevel', {'offset': 0.2}],
        ['EDGE', {3, 4, 5}, 'tri2gaps', 'tri2gaps_result_2', 'bevel', {'offset': 0.2, 'segments': 2}],
        ['EDGE', {3, 4, 5}, 'tri2gaps', 'tri2gaps_result_3', 'bevel', {'offset': 0.2, 'segments': 3}],
        ['EDGE', {3, 4}, 'tri2gaps', 'tri2gaps_result_4', 'bevel', {'offset': 0.2}],
        ['EDGE', {3, 4}, 'tri2gaps', 'tri2gaps_result_5', 'bevel', {'offset': 0.2, 'segments': 2}],
        # 60
        ['EDGE', {3, 4}, 'tri2gaps', 'tri2gaps_result_6', 'bevel', {'offset': 0.2, 'segments': 3}],
        ['EDGE', {3, 4, 5}, 'tri3gaps', 'tri3gaps_result_1', 'bevel', {'offset': 0.2}],
        ['EDGE', {3, 4, 5}, 'tri3gaps', 'tri3gaps_result_2', 'bevel', {'offset': 0.2, 'segments': 2}],
        ['EDGE', {3, 4, 5}, 'tri3gaps', 'tri3gaps_result_3', 'bevel', {'offset': 0.2, 'segments': 3}],
        ['EDGE', {32, 33, 34, 35, 24, 25, 26, 27, 28, 29, 30, 31}, 'cube3', 'cube3_result_1', 'bevel', {'offset': 0.2}],
        # 65
        ['EDGE', {32, 33, 34, 35, 24, 25, 26, 27, 28, 29, 30, 31}, 'cube3', 'cube3_result_2', 'bevel',
         {'offset': 0.2, 'segments': 2}],
        ['EDGE', {32, 35}, 'cube3', 'cube3_result_3', 'bevel', {'offset': 0.2}],
        ['EDGE', {24, 35}, 'cube3', 'cube3_result_4', 'bevel', {'offset': 0.2}],
        ['EDGE', {24, 32, 35}, 'cube3', 'cube3_result_5', 'bevel', {'offset': 0.2, 'segments': 2}],
        ['EDGE', {24, 32, 35}, 'cube3', 'cube3_result_6', 'bevel', {'offset': 0.2, 'segments': 3}],
        # 70
        ['EDGE', {0, 1, 6, 7, 12, 14, 16, 17}, 'Tray', 'Tray_result_1', 'bevel', {'offset': 0.01, 'segments': 2}],
        ['EDGE', {33, 4, 38, 8, 41, 10, 42, 12, 14, 17, 24, 31}, 'Bumptop', 'Bumptop_result_1', 'bevel',
         {'offset': 0.1, 'segments': 4}],
        ['EDGE', {16, 14, 15}, 'Multisegment_test', 'Multisegment_result_1', 'bevel', {'offset': 0.2}],
        ['EDGE', {16, 14, 15}, 'Multisegment_test', 'Multisegment_result_1', 'bevel', {'offset': 0.2}],
        ['EDGE', {19, 20, 23, 15}, 'Window_test', 'Window_result_1', 'bevel', {'offset': 0.05, 'segments': 2}],
        # 75
        ['EDGE', {8}, 'Cube_hn_test', 'Cube_hn_result_1', 'bevel', {'offset': 0.2, 'harden_normals': True}],
        ['EDGE', {4, 7, 39, 27, 30, 31}, 'Blocksteps_test', 'Blocksteps_result_1', 'bevel',
         {'offset': 0.2, 'miter_outer': 'PATCH'}],
        ['EDGE', {4, 7, 39, 27, 30, 31}, 'Blocksteps_test', 'Blocksteps_result_2', 'bevel',
         {'offset': 0.2, 'segments': 2, 'miter_outer': 'PATCH'}],
        ['EDGE', {4, 7, 39, 27, 30, 31}, 'Blocksteps_test', 'Blocksteps_result_3', 'bevel',
         {'offset': 0.2, 'segments': 3, 'miter_outer': 'PATCH'}],
        ['EDGE', {4, 7, 39, 27, 30, 31}, 'Blocksteps_test', 'Blocksteps_result_4', 'bevel',
         {'offset': 0.2, 'miter_outer': 'ARC'}],
        # 80
        ['EDGE', {4, 7, 39, 27, 30, 31}, 'Blocksteps_test', 'Blocksteps_result_5', 'bevel',
         {'offset': 0.2, 'segments': 2, 'miter_outer': 'ARC'}],
        ['EDGE', {4, 7, 39, 27, 30, 31}, 'Blocksteps_test', 'Blocksteps_result_6', 'bevel',
         {'offset': 0.2, 'segments': 3, 'miter_outer': 'ARC'}],
        ['EDGE', {4, 7, 39, 27, 30, 31}, 'Blocksteps_test', 'Blocksteps_result_7', 'bevel',
         {'offset': 0.2, 'miter_outer': 'PATCH', 'miter_inner': 'ARC'}],
        ['EDGE', {4, 7, 39, 27, 30, 31}, 'Blocksteps_test', 'Blocksteps_result_8', 'bevel',
         {'offset': 0.2, 'segments': 2, 'miter_outer': 'PATCH', 'miter_inner': 'ARC'}],
        ['EDGE', {4, 7, 39, 27, 30, 31}, 'Blocksteps2_test', 'Blocksteps2_result_9', 'bevel',
         {'offset': 0.2, 'segments': 2, 'miter_outer': 'ARC'}],
        # 85
        ['EDGE', {4, 7, 39, 27, 30, 31}, 'Blocksteps3_test', 'Blocksteps3_result_10', 'bevel',
         {'offset': 0.2, 'segments': 2, 'miter_outer': 'ARC'}],
        ['EDGE', {4, 7, 39, 27, 30, 31}, 'Blocksteps4_test', 'Blocksteps4_result_11', 'bevel',
         {'offset': 0.2, 'segments': 2, 'miter_outer': 'ARC'}],
        ['EDGE', {4, 7, 39, 27, 30, 31}, 'Blocksteps4_test', 'Blocksteps4_result_12', 'bevel',
         {'offset': 0.2, 'segments': 3, 'miter_outer': 'ARC'}],
        ['EDGE', {1, 7}, 'Spike_test', 'Spike_result_1', 'bevel', {'offset': 0.2, 'segments': 3}]
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
