# SPDX-FileCopyrightText: 2020-2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# To run all tests, use
# BLENDER_VERBOSE=1 ./bin/blender ../blender/tests/data/modeling/object_conversion.blend --python ../blender/tests/python/object_conversion.py -- --run-all-tests
# (that assumes the test is run from a build directory in the same directory as the source code)
import bpy
import os
import sys
import inspect

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import SpecMeshTest, OperatorSpecObjectMode, RunTest


class ConversionTypeTestHelper:
    def __init__(self, tests):
        self.tests = tests
        self._failed_tests_list = []

    def run_all_tests(self):
        """
        Run all tests in self.tests list. Displays all failed tests at bottom.
        """
        for _, each_test in enumerate(self.tests):
            test_name = each_test.test_name
            success = self.run_test(test_name)

            if not success:
                self._failed_tests_list.append(test_name)

        if len(self._failed_tests_list) != 0:
            print("\nFollowing tests failed: {}".format(self._failed_tests_list))

            blender_path = bpy.app.binary_path
            blend_path = bpy.data.filepath
            frame = inspect.stack()[1]
            module = inspect.getmodule(frame[0])
            python_path = module.__file__

            print("Run following command to open Blender and run the failing test:")
            print("{} {} --python {} -- {} {}"
                  .format(blender_path, blend_path, python_path, "--run-test", "<test_name>"))

            raise Exception("Tests {} failed".format(self._failed_tests_list))

    def run_test(self, test_name: str):
        """
        Run a single test from self.tests list.

        :arg test_name: int - name of test
        :return: bool - True if test passed, False otherwise.
        """
        case = None
        for index, each_test in enumerate(self.tests):
            if test_name == each_test.test_name:
                case = self.tests[index]
                break

        if case is None:
            raise Exception('No test called {} found!'.format(test_name))

        test = case
        print("Running test '{}'".format(test.test_name))

        test_object = bpy.data.objects[test.from_object]
        with bpy.context.temp_override(object=test_object, selected_objects=[test_object]):
            bpy.context.view_layer.objects.active = test_object

            selection = test_object.select_get()
            test_object.select_set(True)
            retval = bpy.ops.object.convert(target=test.to_type, keep_original=True)
            test_object.select_set(False)

            if retval != {'FINISHED'}:
                raise RuntimeError("Unexpected operator return value: {}".format(retval))

        resulting_type = bpy.context.view_layer.objects.active.type
        bpy.ops.object.delete()
        if resulting_type != test.resulting_type:
            raise RuntimeError(
                "Converted object does not match expected type.\nTest '{}': Converting '{}' to '{}' expecting '{}' got '{}'\n" .format(
                    test.test_name,
                    test.from_object,
                    test.to_type,
                    test.resulting_type,
                    resulting_type))

        print("Success\n")
        return True


class ConversionPair:
    def __init__(self, test_name, from_object, to_type, resulting_type):
        self.test_name = test_name
        self.from_object = from_object
        self.to_type = to_type
        self.resulting_type = resulting_type


def main():
    tests = [
        SpecMeshTest('Mesh 1', 'Cube', 'Cube_Mesh',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Mesh 2', 'CubeWithEdges', 'CubeWithEdges_Mesh',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Mesh 3', 'Plane', 'Plane_Mesh',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Mesh 4', 'HollowPlane', 'HollowPlane_Mesh',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Mesh 5', 'Suzanne', 'Suzanne_Mesh',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Mesh 6', 'BezierCircle', 'BezierCircle_Mesh',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Mesh 7', 'BezierCurve', 'BezierCurve_Mesh',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
        SpecMeshTest('Mesh 8', 'Text', 'Text_Mesh',
                     [OperatorSpecObjectMode('convert', {'target': 'MESH'})]),
    ]

    type_tests = [
        ConversionPair('Legacy Curves 1', 'Cube', 'CURVE', 'MESH'),
        ConversionPair('Legacy Curves 2', 'CubeWithEdges', 'CURVE', 'CURVE'),
        ConversionPair('Legacy Curves 3', 'Plane', 'CURVE', 'CURVE'),
        ConversionPair('Legacy Curves 4', 'HollowPlane', 'CURVE', 'CURVE'),
        ConversionPair('Legacy Curves 5', 'Suzanne', 'CURVE', 'GREASEPENCIL'),
        ConversionPair('Legacy Curves 6', 'BezierCircle', 'CURVE', 'CURVE'),
        ConversionPair('Legacy Curves 7', 'BezierCurve', 'CURVE', 'CURVE'),
        ConversionPair('Legacy Curves 8', 'Text', 'CURVE', 'CURVE'),
        ConversionPair('Curves 1', 'Cube', 'CURVES', 'MESH'),
        ConversionPair('Curves 2', 'CubeWithEdges', 'CURVES', 'MESH'),
        ConversionPair('Curves 3', 'Plane', 'CURVES', 'MESH'),
        ConversionPair('Curves 4', 'HollowPlane', 'CURVES', 'MESH'),
        ConversionPair('Curves 5', 'Suzanne', 'CURVES', 'CURVES'),
        ConversionPair('Curves 6', 'BezierCircle', 'CURVES', 'CURVES'),
        ConversionPair('Curves 7', 'BezierCurve', 'CURVES', 'CURVES'),
        ConversionPair('Curves 8', 'Text', 'CURVES', 'CURVES'),
        ConversionPair('GreasePencil 1', 'Cube', 'GREASEPENCIL', 'GREASEPENCIL'),
        ConversionPair('GreasePencil 2', 'CubeWithEdges', 'GREASEPENCIL', 'GREASEPENCIL'),
        ConversionPair('GreasePencil 3', 'Plane', 'GREASEPENCIL', 'GREASEPENCIL'),
        ConversionPair('GreasePencil 4', 'HollowPlane', 'GREASEPENCIL', 'GREASEPENCIL'),
        ConversionPair('GreasePencil 5', 'Suzanne', 'GREASEPENCIL', 'GREASEPENCIL'),
        ConversionPair('GreasePencil 6', 'BezierCircle', 'GREASEPENCIL', 'GREASEPENCIL'),
        ConversionPair('GreasePencil 7', 'BezierCurve', 'GREASEPENCIL', 'GREASEPENCIL'),
        ConversionPair('GreasePencil 8', 'Text', 'GREASEPENCIL', 'GREASEPENCIL'),
        ConversionPair('GreasePencil 9', 'Suzanne_Curves', 'GREASEPENCIL', 'GREASEPENCIL'),
        ConversionPair('GreasePencil 10', 'BezierCircle_Curves', 'GREASEPENCIL', 'GREASEPENCIL'),
        ConversionPair('GreasePencil 11', 'BezierCurve_Curves', 'GREASEPENCIL', 'GREASEPENCIL'),
    ]

    operator_test = RunTest(tests)
    all_type_tests = ConversionTypeTestHelper(type_tests)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            operator_test.do_compare = True
            operator_test.run_all_tests()
            all_type_tests.run_all_tests()
            break
        elif cmd == "--run-test":
            name = command[i + 1]
            operator_test.do_compare = False
            operator_test.run_test(name)
            break


if __name__ == "__main__":
    main()
