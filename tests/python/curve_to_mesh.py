# SPDX-FileCopyrightText: 2020-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# To run all tests, use
# BLENDER_VERBOSE=1 ./bin/blender ../tests/files/modeling/curve_to_mesh.blend --python ../blender/tests/python/bl_curve_to_mesh.py -- --run-all-tests
# (that assumes the test is run from a build directory in the same directory as the source code)
import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import SpecMeshTest, OperatorSpec, RunTest


def main():
    tests = [
        SpecMeshTest('2D Non Cyclic', 'test2DNonCyclic', 'expected2DNonCyclic',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('2D NURBS With Tail', 'test2DNURBSWithTail', 'expected2DNURBSWithTail',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('2D Shape With Hole', 'test2DShapeWithHole', 'expected2DShapeWithHole',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('2D Simple Lower Res', 'test2DSimpleLowerRes', 'expected2DSimpleLowerRes',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('2D Simple Low Res', 'test2DSimpleLowRes', 'expected2DSimpleLowRes',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('2D Square', 'test2DSquare', 'expected2DSquare',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('2D Extrude', 'test2DExtrude', 'expected2DExtrude',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Back', 'testBevelBack', 'expectedBevelBack',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Back Low Res', 'testBevelBackLowRes', 'expectedBevelBackLowRes',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Extrude Back', 'testBevelExtrudeBack', 'expectedBevelExtrudeBack',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Extrude Front', 'testBevelExtrudeFront', 'expectedBevelExtrudeFront',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Extrude Full', 'testBevelExtrudeFull', 'expectedBevelExtrudeFull',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Extrude Half', 'testBevelExtrudeHalf', 'expectedBevelExtrudeHalf',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Front', 'testBevelFront', 'expectedBevelFront',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Front Low Res', 'testBevelFrontLowRes', 'expectedBevelFrontLowRes',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Full', 'testBevelFull', 'expectedBevelFull',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Full Low Res', 'testBevelFullLowRes', 'expectedBevelFullLowRes',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Half', 'testBevelHalf', 'expectedBevelHalf',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Bevel Half Low Res', 'testBevelHalfLowRes', 'expectedBevelHalfLowRes',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps None', 'testCapsNone', 'expectedCapsNone',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Object Bevel', 'testCapsObjectBevel', 'expectedCapsObjectBevel',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Profile Bevel', 'testCapsProfileBevel', 'expectedCapsProfileBevel',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Profile Bevel Half', 'testCapsProfileBevelHalf', 'expectedCapsProfileBevelHalf',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Profile Bevel Quarter', 'testCapsProfileBevelQuarter', 'expectedCapsProfileBevelQuarter',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Round Bevel', 'testCapsRoundBevel', 'expectedCapsRoundBevel',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Round Bevel Extrude', 'testCapsRoundBevelExtrude', 'expectedCapsRoundBevelExtrude',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Round Bevel Half', 'testCapsRoundBevelHalf', 'expectedCapsRoundBevelHalf',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Caps Round Bevel Quarter', 'testCapsRoundBevelQuarter', 'expectedCapsRoundBevelQuarter',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Extrude Back', 'testExtrudeBack', 'expectedExtrudeBack',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Extrude Front', 'testExtrudeFront', 'expectedExtrudeFront',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Extrude Full', 'testExtrudeFull', 'expectedExtrudeFull',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
        SpecMeshTest('Extrude Half', 'testExtrudeHalf', 'expectedExtrudeHalf',
                     [OperatorSpec('OBJECT', 'object.convert', {'target': 'MESH'})]),
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
