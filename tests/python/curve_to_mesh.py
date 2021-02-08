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
# BLENDER_VERBOSE=1 ./bin/blender ../lib/tests/modeling/curve_to_mesh.blend --python ../blender/tests/python/bl_curve_to_mesh.py -- --run-all-tests
# (that assumes the test is run from a build directory in the same directory as the source code)
import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import MeshTest, OperatorSpecObjectMode, RunTest


def main():
    tests = [
        MeshTest('2D Non Cyclic', 'test2DNonCyclic', 'expected2DNonCyclic',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('2D NURBS With Tail', 'test2DNURBSWithTail', 'expected2DNURBSWithTail',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('2D Shape With Hole', 'test2DShapeWithHole', 'expected2DShapeWithHole',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('2D Simple Lower Res', 'test2DSimpleLowerRes', 'expected2DSimpleLowerRes',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('2D Simple Low Res', 'test2DSimpleLowRes', 'expected2DSimpleLowRes',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('2D Square', 'test2DSquare', 'expected2DSquare',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Bevel Back', 'testBevelBack', 'expectedBevelBack',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Bevel Back Low Res', 'testBevelBackLowRes', 'expectedBevelBackLowRes',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Bevel Extrude Back', 'testBevelExtrudeBack', 'expectedBevelExtrudeBack',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Bevel Extrude Front', 'testBevelExtrudeFront', 'expectedBevelExtrudeFront',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Bevel Extrude Full', 'testBevelExtrudeFull', 'expectedBevelExtrudeFull',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Bevel Extrude Half', 'testBevelExtrudeHalf', 'expectedBevelExtrudeHalf',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Bevel Front', 'testBevelFront', 'expectedBevelFront',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Bevel Front Low Res', 'testBevelFrontLowRes', 'expectedBevelFrontLowRes',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Bevel Full', 'testBevelFull', 'expectedBevelFull',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Bevel Full Low Res', 'testBevelFullLowRes', 'expectedBevelFullLowRes',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Bevel Half', 'testBevelHalf', 'expectedBevelHalf',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Bevel Half Low Res', 'testBevelHalfLowRes', 'expectedBevelHalfLowRes',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Caps None', 'testCapsNone', 'expectedCapsNone',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Caps Object Bevel', 'testCapsObjectBevel', 'expectedCapsObjectBevel',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Caps Profile Bevel', 'testCapsProfileBevel', 'expectedCapsProfileBevel',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Caps Profile Bevel Half', 'testCapsProfileBevelHalf', 'expectedCapsProfileBevelHalf',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Caps Profile Bevel Quarter', 'testCapsProfileBevelQuarter', 'expectedCapsProfileBevelQuarter',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Caps Round Bevel', 'testCapsRoundBevel', 'expectedCapsRoundBevel',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Caps Round Bevel Extrude', 'testCapsRoundBevelExtrude', 'expectedCapsRoundBevelExtrude',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Caps Round Bevel Half', 'testCapsRoundBevelHalf', 'expectedCapsRoundBevelHalf',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Caps Round Bevel Quarter', 'testCapsRoundBevelQuarter', 'expectedCapsRoundBevelQuarter',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Extrude Back', 'testExtrudeBack', 'expectedExtrudeBack',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Extrude Front', 'testExtrudeFront', 'expectedExtrudeFront',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Extrude Full', 'testExtrudeFull', 'expectedExtrudeFull',
                 [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        MeshTest('Extrude Half', 'testExtrudeHalf', 'expectedExtrudeHalf',
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
