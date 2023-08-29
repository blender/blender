# SPDX-FileCopyrightText: 2020-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# To run all tests, use
# BLENDER_VERBOSE=1 ./bin/blender ../lib/tests/modeling/curve_to_mesh.blend --python ../blender/tests/python/bl_curve_to_mesh.py -- --run-all-tests
# (that assumes the test is run from a build directory in the same directory as the source code)
import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import SpecMeshTest, OperatorSpecObjectMode, RunTest


def main():
    tests = [
        SpecMeshTest('2D Non Cyclic', 'test2DNonCyclic', 'expected2DNonCyclic',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('2D NURBS With Tail', 'test2DNURBSWithTail', 'expected2DNURBSWithTail',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('2D Shape With Hole', 'test2DShapeWithHole', 'expected2DShapeWithHole',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('2D Simple Lower Res', 'test2DSimpleLowerRes', 'expected2DSimpleLowerRes',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('2D Simple Low Res', 'test2DSimpleLowRes', 'expected2DSimpleLowRes',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('2D Square', 'test2DSquare', 'expected2DSquare',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('2D Extrude', 'test2DExtrude', 'expected2DExtrude',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Back', 'testBevelBack', 'expectedBevelBack',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Back Low Res', 'testBevelBackLowRes', 'expectedBevelBackLowRes',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Extrude Back', 'testBevelExtrudeBack', 'expectedBevelExtrudeBack',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Extrude Front', 'testBevelExtrudeFront', 'expectedBevelExtrudeFront',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Extrude Full', 'testBevelExtrudeFull', 'expectedBevelExtrudeFull',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Extrude Half', 'testBevelExtrudeHalf', 'expectedBevelExtrudeHalf',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Front', 'testBevelFront', 'expectedBevelFront',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Front Low Res', 'testBevelFrontLowRes', 'expectedBevelFrontLowRes',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Full', 'testBevelFull', 'expectedBevelFull',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Full Low Res', 'testBevelFullLowRes', 'expectedBevelFullLowRes',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Half', 'testBevelHalf', 'expectedBevelHalf',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Half Low Res', 'testBevelHalfLowRes', 'expectedBevelHalfLowRes',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps None', 'testCapsNone', 'expectedCapsNone',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Object Bevel', 'testCapsObjectBevel', 'expectedCapsObjectBevel',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Profile Bevel', 'testCapsProfileBevel', 'expectedCapsProfileBevel',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Profile Bevel Half', 'testCapsProfileBevelHalf', 'expectedCapsProfileBevelHalf',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Profile Bevel Quarter', 'testCapsProfileBevelQuarter', 'expectedCapsProfileBevelQuarter',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Round Bevel', 'testCapsRoundBevel', 'expectedCapsRoundBevel',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Round Bevel Extrude', 'testCapsRoundBevelExtrude', 'expectedCapsRoundBevelExtrude',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Round Bevel Half', 'testCapsRoundBevelHalf', 'expectedCapsRoundBevelHalf',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Round Bevel Quarter', 'testCapsRoundBevelQuarter', 'expectedCapsRoundBevelQuarter',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Extrude Back', 'testExtrudeBack', 'expectedExtrudeBack',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Extrude Front', 'testExtrudeFront', 'expectedExtrudeFront',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Extrude Full', 'testExtrudeFull', 'expectedExtrudeFull',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Extrude Half', 'testExtrudeHalf', 'expectedExtrudeHalf',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
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
