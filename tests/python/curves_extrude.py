# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from abc import ABC, abstractmethod
import bpy
import os
import sys

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import RunTest


class CurvesTest(ABC):

    def __init__(self, test_object_name, exp_object_name, test_name=None):
        self.test_object_name = test_object_name
        self.exp_object_name = exp_object_name
        self.test_object = bpy.data.objects[self.test_object_name]
        self.expected_object = bpy.data.objects[self.exp_object_name]
        self.verbose = os.getenv("BLENDER_VERBOSE") is not None

        if test_name:
            self.test_name = test_name
        else:
            filepath = bpy.data.filepath
            self.test_name = bpy.path.display_name_from_filepath(filepath)
        self._failed_tests_list = []

    def create_evaluated_object(self):
        """
        Creates an evaluated object.
        """
        bpy.context.view_layer.objects.active = self.test_object

        # Duplicate test object.
        bpy.ops.object.mode_set(mode="OBJECT")
        bpy.ops.object.select_all(action="DESELECT")
        bpy.context.view_layer.objects.active = self.test_object

        self.test_object.select_set(True)
        bpy.ops.object.duplicate()
        self.evaluated_object = bpy.context.active_object
        self.evaluated_object.name = "evaluated_object"

    @staticmethod
    def _print_result(result):
        """
        Prints the comparison, selection and validation result.
        """
        print("Results:")
        for key in result:
            print("{} : {}".format(key, result[key][1]))
        print()

    def run_test(self):
        """
        Runs a single test, runs it again if test file is updated.
        """
        print("\nSTART {} test.".format(self.test_name))

        self.create_evaluated_object()
        self.apply_operations()

        result = self.compare_objects(self.evaluated_object, self.expected_object)

        # Initializing with True to get correct resultant of result_code booleans.
        success = True
        inside_loop_flag = False
        for key in result:
            inside_loop_flag = True
            success = success and result[key][0]

        # Check "success" is actually evaluated and is not the default True value.
        if not inside_loop_flag:
            success = False

        if success:
            self.print_passed_test_result(result)
            # Clean up.
            if self.verbose:
                print("Cleaning up...")
            # Delete evaluated_test_object.
            bpy.ops.object.delete()
            return True

        else:
            self.print_failed_test_result(result)
            return False

    @abstractmethod
    def apply_operations(self, evaluated_test_object_name):
        pass

    @staticmethod
    def compare_curves(evaluated_curves, expected_curves):
        if len(evaluated_curves.attributes.items()) != len(expected_curves.attributes.items()):
            print("Attribute count doesn't match")

        for a_idx, attribute in evaluated_curves.attributes.items():
            expected_attribute = expected_curves.attributes[a_idx]

            if len(attribute.data.items()) != len(expected_attribute.data.items()):
                print("Attribute data length doesn't match")

            value_attr_name = ('vector' if attribute.data_type == 'FLOAT_VECTOR'
                               or attribute.data_type == 'FLOAT2' else
                               'color' if attribute.data_type == 'FLOAT_COLOR' else 'value')

            for v_idx, attribute_value in attribute.data.items():
                if getattr(
                        attribute_value,
                        value_attr_name) != getattr(
                        expected_attribute.data[v_idx],
                        value_attr_name):
                    print("Attribute '{}' values do not match".format(attribute.name))
                    return False

        return True

    def compare_objects(self, evaluated_object, expected_object):
        result_codes = {}

        equal = self.compare_curves(evaluated_object.data, expected_object.data)

        result_codes['Curves Comparison'] = (equal, evaluated_object.data)
        return result_codes

    def print_failed_test_result(self, result):
        """
        Print results for failed test.
        """
        print("FAILED {} test with the following: ".format(self.test_name))

    def print_passed_test_result(self, result):
        """
        Print results for passing test.
        """
        print("PASSED {} test successfully.".format(self.test_name))


class CurvesOpTest(CurvesTest):

    def __init__(self, test_name, test_object_name, exp_object_name, operators_stack):
        super().__init__(test_object_name, exp_object_name, test_name)
        self.operators_stack = operators_stack

    def apply_operations(self):
        for operator_name in self.operators_stack:
            bpy.ops.object.mode_set(mode='EDIT')
            curves_operator = getattr(bpy.ops.curves, operator_name)

            try:
                retval = curves_operator()
            except AttributeError:
                raise AttributeError("bpy.ops.curves has no attribute {}".format(operator_name))
            except TypeError as ex:
                raise TypeError("Incorrect operator parameters {!r} raised {!r}".format([], ex))

            if retval != {'FINISHED'}:
                raise RuntimeError("Unexpected operator return value: {}".format(operator_name))
            bpy.ops.object.mode_set(mode='OBJECT')


def main():
    tests = [
        CurvesOpTest("Extrude 1 Point Curve", "a_test1PointCurve", "a_test1PointCurveExpected", ['extrude']),
        CurvesOpTest("Extrude Middle Points", "b_testMiddlePoints", "b_testMiddlePointsExpected", ['extrude']),
        CurvesOpTest("Extrude End Points", "c_testEndPoints", "c_testEndPointsExpected", ['extrude']),
        CurvesOpTest("Extrude Neighbors In Separate Curves", "d_testNeighborsInCurves", "d_testNeighborsInCurvesExpected", ['extrude']),
        CurvesOpTest("Extrude Edge Curves", "e_testEdgeCurves", "e_testEdgeCurvesExpected", ['extrude']),
        CurvesOpTest("Extrude Middle Curve", "f_testMiddleCurve", "f_testMiddleCurveExpected", ['extrude']),
        CurvesOpTest("Extrude All Points", "g_testAllPoints", "g_testAllPointsExpected", ['extrude'])
    ]

    curves_extrude_test = RunTest(tests)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            curves_extrude_test.do_compare = True
            curves_extrude_test.run_all_tests()
            break
        elif cmd == "--run-test":
            curves_extrude_test.do_compare = False
            name = command[i + 1]
            curves_extrude_test.run_test(name)
            break


if __name__ == "__main__":
    main()
