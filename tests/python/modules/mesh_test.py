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

# A framework to run regression tests on mesh modifiers and operators based on howardt's mesh_ops_test.py
#
# General idea:
# A test is:
#    Object mode
#    Select <test_object>
#    Duplicate the object
#    Select the object
#    Apply operation for each operation in <operations_stack> with given parameters
#    (an operation is either a modifier or an operator)
#    test_mesh = <test_object>.data
#    run test_mesh.unit_test_compare(<expected object>.data)
#    delete the duplicate object
#
# The words in angle brackets are parameters of the test, and are specified in
# the main class MeshTest.
#
# If the environment variable BLENDER_TEST_UPDATE is set to 1, the <expected_object>
# is updated with the new test result.
# Tests are verbose when the environment variable BLENDER_VERBOSE is set.


import bpy
import functools
import inspect
import os


# Output from this module and from blender itself will occur during tests.
# We need to flush python so that the output is properly interleaved, otherwise
# blender's output for one test will end up showing in the middle of another test...
print = functools.partial(print, flush=True)


class ModifierSpec:
    """
    Holds one modifier and its parameters.
    """

    def __init__(self, modifier_name: str, modifier_type: str, modifier_parameters: dict):
        """
        Constructs a modifier spec.
        :param modifier_name: str - name of object modifier, e.g. "myFirstSubsurfModif"
        :param modifier_type: str - type of object modifier, e.g. "SUBSURF"
        :param modifier_parameters: dict - {name : val} dictionary giving modifier parameters, e.g. {"quality" : 4}
        """
        self.modifier_name = modifier_name
        self.modifier_type = modifier_type
        self.modifier_parameters = modifier_parameters

    def __str__(self):
        return "Modifier: " + self.modifier_name + " of type " + self.modifier_type + \
               " with parameters: " + str(self.modifier_parameters)


class PhysicsSpec:
    """
    Holds one Physics modifier and its parameters.
    """

    def __init__(self, modifier_name: str, modifier_type: str, modifier_parameters: dict, frame_end: int):
        """
        Constructs a physics spec.
        :param modifier_name: str - name of object modifier, e.g. "Cloth"
        :param modifier_type: str - type of object modifier, e.g. "CLOTH"
        :param modifier_parameters: dict - {name : val} dictionary giving modifier parameters, e.g. {"quality" : 4}
        :param frame_end:int - the last frame of the simulation at which it is baked
        """
        self.modifier_name = modifier_name
        self.modifier_type = modifier_type
        self.modifier_parameters = modifier_parameters
        self.frame_end = frame_end

    def __str__(self):
        return "Physics Modifier: " + self.modifier_name + " of type " + self.modifier_type + \
               " with parameters: " + str(self.modifier_parameters) + " with frame end: " + str(self.frame_end)

class OperatorSpec:
    """
    Holds one operator and its parameters.
    """

    def __init__(self, operator_name: str, operator_parameters: dict, select_mode: str, selection: set):
        """
        Constructs an operatorSpec. Raises ValueError if selec_mode is invalid.
        :param operator_name: str - name of mesh operator from bpy.ops.mesh, e.g. "bevel" or "fill"
        :param operator_parameters: dict - {name : val} dictionary containing operator parameters.
        :param select_mode: str - mesh selection mode, must be either 'VERT', 'EDGE' or 'FACE'
        :param selection: set - set of vertices/edges/faces indices to select, e.g. [0, 9, 10].
        """
        self.operator_name = operator_name
        self.operator_parameters = operator_parameters
        if select_mode not in ['VERT', 'EDGE', 'FACE']:
            raise ValueError("select_mode must be either {}, {} or {}".format('VERT', 'EDGE', 'FACE'))
        self.select_mode = select_mode
        self.selection = selection

    def __str__(self):
        return "Operator: " + self.operator_name + " with parameters: " + str(self.operator_parameters) + \
               " in selection mode: " + self.select_mode + ", selecting " + str(self.selection)


class MeshTest:
    """
    A mesh testing class targeted at testing modifiers and operators on a single object.
    It holds a stack of mesh operations, i.e. modifiers or operators. The test is executed using
    the public method run_test().
    """

    def __init__(self, test_object_name: str, expected_object_name: str, operations_stack=None, apply_modifiers=False, threshold=None):
        """
        Constructs a MeshTest object. Raises a KeyError if objects with names expected_object_name
        or test_object_name don't exist.
        :param test_object: str - Name of object of mesh type to run the operations on.
        :param expected_object: str - Name of object of mesh type that has the expected
                                geometry after running the operations.
        :param operations_stack: list - stack holding operations to perform on the test_object.
        :param apply_modifier: bool - True if we want to apply the modifiers right after adding them to the object.
                               This affects operations of type ModifierSpec only.
        """
        if operations_stack is None:
            operations_stack = []
        for operation in operations_stack:
            if not (isinstance(operation, ModifierSpec) or isinstance(operation, OperatorSpec)):
                raise ValueError("Expected operation of type {} or {}. Got {}".
                                 format(type(ModifierSpec), type(OperatorSpec),
                                        type(operation)))
        self.operations_stack = operations_stack
        self.apply_modifier = apply_modifiers
        self.threshold = threshold

        self.verbose = os.environ.get("BLENDER_VERBOSE") is not None
        self.update = os.getenv('BLENDER_TEST_UPDATE') is not None

        # Initialize test objects.
        objects = bpy.data.objects
        self.test_object = objects[test_object_name]
        self.expected_object = objects[expected_object_name]
        if self.verbose:
            print("Found test object {}".format(test_object_name))
            print("Found test object {}".format(expected_object_name))

        # Private flag to indicate whether the blend file was updated after the test.
        self._test_updated = False

    def set_test_object(self, test_object_name):
        """
        Set test object for the test. Raises a KeyError if object with given name does not exist.
        :param test_object_name: name of test object to run operations on.
        """
        objects = bpy.data.objects
        self.test_object = objects[test_object_name]

    def set_expected_object(self, expected_object_name):
        """
        Set expected object for the test. Raises a KeyError if object with given name does not exist
        :param expected_object_name: Name of expected object.
        """
        objects = bpy.data.objects
        self.expected_object = objects[expected_object_name]

    def add_modifier(self, modifier_spec: ModifierSpec):
        """
        Add a modifier to the operations stack.
        :param modifier_spec: modifier to add to the operations stack
        """
        self.operations_stack.append(modifier_spec)
        if self.verbose:
            print("Added modififier {}".format(modifier_spec))

    def add_operator(self, operator_spec: OperatorSpec):
        """
        Adds an operator to the operations stack.
        :param operator_spec: OperatorSpec - operator to add to the operations stack.
        """
        self.operations_stack.append(operator_spec)

    def _on_failed_test(self, compare_result, validation_success, evaluated_test_object):
        if self.update and validation_success:
            if self.verbose:
                print("Test failed expectantly. Updating expected mesh...")

            # Replace expected object with object we ran operations on, i.e. evaluated_test_object.
            evaluated_test_object.location = self.expected_object.location
            expected_object_name = self.expected_object.name

            bpy.data.objects.remove(self.expected_object, do_unlink=True)
            evaluated_test_object.name = expected_object_name

            # Save file
            bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath)

            self._test_updated = True

            # Set new expected object.
            self.expected_object = evaluated_test_object
            return True

        else:
            print("Test comparison result: {}".format(compare_result))
            print("Test validation result: {}".format(validation_success))
            print("Resulting object mesh '{}' did not match expected object '{}' from file {}".
                  format(evaluated_test_object.name, self.expected_object.name, bpy.data.filepath))

            return False

    def is_test_updated(self):
        """
        Check whether running the test with BLENDER_TEST_UPDATE actually modified the .blend test file.
        :return: Bool - True if blend file has been updated. False otherwise.
        """
        return self._test_updated

    def _apply_modifier(self, test_object, modifier_spec: ModifierSpec):
        """
        Add modifier to object and apply (if modifier_spec.apply_modifier is True)
        :param test_object: bpy.types.Object - Blender object to apply modifier on.
        :param modifier_spec: ModifierSpec - ModifierSpec object with parameters
        """
        modifier = test_object.modifiers.new(modifier_spec.modifier_name,
                                             modifier_spec.modifier_type)
        if self.verbose:
            print("Created modifier '{}' of type '{}'.".
                  format(modifier_spec.modifier_name, modifier_spec.modifier_type))

        for param_name in modifier_spec.modifier_parameters:
            try:
                setattr(modifier, param_name, modifier_spec.modifier_parameters[param_name])
                if self.verbose:
                    print("\t set parameter '{}' with value '{}'".
                          format(param_name, modifier_spec.modifier_parameters[param_name]))
            except AttributeError:
                # Clean up first
                bpy.ops.object.delete()
                raise AttributeError("Modifier '{}' has no parameter named '{}'".
                                     format(modifier_spec.modifier_type, param_name))

        if self.apply_modifier:
            bpy.ops.object.modifier_apply(modifier=modifier_spec.modifier_name)


    def _bake_current_simulation(self, obj, test_mod_type, test_mod_name, frame_end):
        for scene in bpy.data.scenes:
            for modifier in obj.modifiers:
                if modifier.type == test_mod_type:
                    obj.modifiers[test_mod_name].point_cache.frame_end = frame_end
                    override = {'scene': scene, 'active_object': obj, 'point_cache': modifier.point_cache}
                    bpy.ops.ptcache.bake(override, bake=True)
                    break

    def _apply_physics_settings(self, test_object, physics_spec: PhysicsSpec):
        """
        Apply Physics settings to test objects.
        """
        scene = bpy.context.scene
        scene.frame_set(1)
        modifier = test_object.modifiers.new(physics_spec.modifier_name,
                                             physics_spec.modifier_type)
        physics_setting = modifier.settings
        if self.verbose:
            print("Created modifier '{}' of type '{}'.".
                  format(physics_spec.modifier_name, physics_spec.modifier_type))


        for param_name in physics_spec.modifier_parameters:
            try:
                setattr(physics_setting, param_name, physics_spec.modifier_parameters[param_name])
                if self.verbose:
                    print("\t set parameter '{}' with value '{}'".
                          format(param_name, physics_spec.modifier_parameters[param_name]))
            except AttributeError:
                # Clean up first
                bpy.ops.object.delete()
                raise AttributeError("Modifier '{}' has no parameter named '{}'".
                                     format(physics_spec.modifier_type, param_name))

        scene.frame_set(physics_spec.frame_end + 1)

        self._bake_current_simulation(test_object, physics_spec.modifier_type, physics_spec.modifier_name, physics_spec.frame_end)
        if self.apply_modifier:
            bpy.ops.object.modifier_apply(modifier=physics_spec.modifier_name)


    def _apply_operator(self, test_object, operator: OperatorSpec):
        """
        Apply operator on test object.
        :param test_object: bpy.types.Object - Blender object to apply operator on.
        :param operator: OperatorSpec - OperatorSpec object with parameters.
        """
        mesh = test_object.data
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='DESELECT')
        bpy.ops.object.mode_set(mode='OBJECT')

        # Do selection.
        bpy.context.tool_settings.mesh_select_mode = (operator.select_mode == 'VERT',
                                                      operator.select_mode == 'EDGE',
                                                      operator.select_mode == 'FACE')
        for index in operator.selection:
            if operator.select_mode == 'VERT':
                mesh.vertices[index].select = True
            elif operator.select_mode == 'EDGE':
                mesh.edges[index].select = True
            elif operator.select_mode == 'FACE':
                mesh.polygons[index].select = True
            else:
                raise ValueError("Invalid selection mode")

        # Apply operator in edit mode.
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_mode(type=operator.select_mode)
        mesh_operator = getattr(bpy.ops.mesh, operator.operator_name)
        if not mesh_operator:
            raise AttributeError("No mesh operator {}".format(operator.operator_name))
        retval = mesh_operator(**operator.operator_parameters)
        if retval != {'FINISHED'}:
            raise RuntimeError("Unexpected operator return value: {}".format(retval))
        if self.verbose:
            print("Applied operator {}".format(operator))

        bpy.ops.object.mode_set(mode='OBJECT')

    def run_test(self):
        """
        Apply operations in self.operations_stack on self.test_object and compare the
        resulting mesh with self.expected_object.data
        :return: bool - True if the test passed, False otherwise.
        """
        self._test_updated = False
        bpy.context.view_layer.objects.active = self.test_object

        # Duplicate test object.
        bpy.ops.object.mode_set(mode="OBJECT")
        bpy.ops.object.select_all(action="DESELECT")
        bpy.context.view_layer.objects.active = self.test_object

        self.test_object.select_set(True)
        bpy.ops.object.duplicate()
        evaluated_test_object = bpy.context.active_object
        evaluated_test_object.name = "evaluated_object"
        if self.verbose:
            print(evaluated_test_object.name, "is set to active")

        # Add modifiers and operators.
        for operation in self.operations_stack:
            if isinstance(operation, ModifierSpec):
                self._apply_modifier(evaluated_test_object, operation)

            elif isinstance(operation, OperatorSpec):
                self._apply_operator(evaluated_test_object, operation)

            elif isinstance(operation, PhysicsSpec):
                self._apply_physics_settings(evaluated_test_object, operation)
            else:
                raise ValueError("Expected operation of type {} or {} or {}. Got {}".
                                 format(type(ModifierSpec), type(OperatorSpec), type(PhysicsSpec),
                                        type(operation)))

        # Compare resulting mesh with expected one.
        if self.verbose:
            print("Comparing expected mesh with resulting mesh...")
        evaluated_test_mesh = evaluated_test_object.data
        expected_mesh = self.expected_object.data
        if self.threshold:
            compare_result = evaluated_test_mesh.unit_test_compare(mesh=expected_mesh, threshold=self.threshold)
        else:
            compare_result = evaluated_test_mesh.unit_test_compare(mesh=expected_mesh)
        compare_success = (compare_result == 'Same')

        # Also check if invalid geometry (which is never expected) had to be corrected...
        validation_success = evaluated_test_mesh.validate(verbose=True) == False

        if compare_success and validation_success:
            if self.verbose:
                print("Success!")

            # Clean up.
            if self.verbose:
                print("Cleaning up...")
            # Delete evaluated_test_object.
            bpy.ops.object.delete()
            return True

        else:
            return self._on_failed_test(compare_result, validation_success, evaluated_test_object)


class OperatorTest:
    """
    Helper class that stores and executes operator tests.

    Example usage:

    >>> tests = [
    >>>     ['FACE', {0, 1, 2, 3, 4, 5}, 'Cubecube', 'Cubecube_result_1', 'intersect_boolean', {'operation': 'UNION'}],
    >>>     ['FACE', {0, 1, 2, 3, 4, 5}, 'Cubecube', 'Cubecube_result_2', 'intersect_boolean', {'operation': 'INTERSECT'}],
    >>> ]
    >>> operator_test = OperatorTest(tests)
    >>> operator_test.run_all_tests()
    """

    def __init__(self, operator_tests):
        """
        Constructs an operator test.
        :param operator_tests: list - list of operator test cases. Each element in the list must contain the following
         in the correct order:
             1) select_mode: str - mesh selection mode, must be either 'VERT', 'EDGE' or 'FACE'
             2) selection: set - set of vertices/edges/faces indices to select, e.g. [0, 9, 10].
             3) test_object_name: bpy.Types.Object - test object
             4) expected_object_name: bpy.Types.Object - expected object
             5) operator_name: str - name of mesh operator from bpy.ops.mesh, e.g. "bevel" or "fill"
             6) operator_parameters: dict - {name : val} dictionary containing operator parameters.
        """
        self.operator_tests = operator_tests
        self.verbose = os.environ.get("BLENDER_VERBOSE") is not None
        self._failed_tests_list = []

    def run_test(self, index: int):
        """
        Run a single test from operator_tests list
        :param index: int - index of test
        :return: bool - True if test is successful. False otherwise.
        """
        case = self.operator_tests[index]
        if len(case) != 6:
            raise ValueError("Expected exactly 6 parameters for each test case, got {}".format(len(case)))
        select_mode = case[0]
        selection = case[1]
        test_object_name = case[2]
        expected_object_name = case[3]
        operator_name = case[4]
        operator_parameters = case[5]

        operator_spec = OperatorSpec(operator_name, operator_parameters, select_mode, selection)

        test = MeshTest(test_object_name, expected_object_name)
        test.add_operator(operator_spec)

        success = test.run_test()
        if test.is_test_updated():
            # Run the test again if the blend file has been updated.
            success = test.run_test()
        return success

    def run_all_tests(self):
        for index, _ in enumerate(self.operator_tests):
            if self.verbose:
                print()
                print("Running test {}...".format(index))
            success = self.run_test(index)

            if not success:
                self._failed_tests_list.append(index)

        if len(self._failed_tests_list) != 0:
            print("Following tests failed: {}".format(self._failed_tests_list))

            blender_path = bpy.app.binary_path
            blend_path = bpy.data.filepath
            frame = inspect.stack()[1]
            module = inspect.getmodule(frame[0])
            python_path = module.__file__

            print("Run following command to open Blender and run the failing test:")
            print("{} {} --python {} -- {} {}"
                  .format(blender_path, blend_path, python_path, "--run-test", "<test_index>"))

            raise Exception("Tests {} failed".format(self._failed_tests_list))


class ModifierTest:
    """
    Helper class that stores and executes modifier tests.

    Example usage:

    >>> modifier_list = [
    >>>     ModifierSpec("firstSUBSURF", "SUBSURF", {"quality": 5}),
    >>>     ModifierSpec("firstSOLIDIFY", "SOLIDIFY", {"thickness_clamp": 0.9, "thickness": 1})
    >>> ]
    >>> tests = [
    >>>     ["testCube", "expectedCube", modifier_list],
    >>>     ["testCube_2", "expectedCube_2", modifier_list]
    >>> ]
    >>> modifiers_test = ModifierTest(tests)
    >>> modifiers_test.run_all_tests()
    """

    def __init__(self, modifier_tests: list, apply_modifiers=False, threshold=None):
        """
        Construct a modifier test.
        :param modifier_tests: list - list of modifier test cases. Each element in the list must contain the following
         in the correct order:
             1) test_object_name: bpy.Types.Object - test object
             2) expected_object_name: bpy.Types.Object - expected object
             3) modifiers: list - list of mesh_test.ModifierSpec objects.
        """
        self.modifier_tests = modifier_tests
        self.apply_modifiers = apply_modifiers
        self.threshold = threshold
        self.verbose = os.environ.get("BLENDER_VERBOSE") is not None
        self._failed_tests_list = []

    def run_test(self, index: int):
        """
        Run a single test from self.modifier_tests list
        :param index: int - index of test
        :return: bool - True if test passed, False otherwise.
        """
        case = self.modifier_tests[index]
        if len(case) != 3:
            raise ValueError("Expected exactly 3 parameters for each test case, got {}".format(len(case)))
        test_object_name = case[0]
        expected_object_name = case[1]
        spec_list = case[2]

        test = MeshTest(test_object_name, expected_object_name, threshold=self.threshold)
        if self.apply_modifiers:
            test.apply_modifier = True

        for modifier_spec in spec_list:
            test.add_modifier(modifier_spec)

        success = test.run_test()
        if test.is_test_updated():
            # Run the test again if the blend file has been updated.
            success = test.run_test()

        return success

    def run_all_tests(self):
        """
        Run all tests in self.modifiers_tests list. Raises an exception if one the tests fails.
        """
        for index, _ in enumerate(self.modifier_tests):
            if self.verbose:
                print()
                print("Running test {}...\n".format(index))
            success = self.run_test(index)

            if not success:
                self._failed_tests_list.append(index)

        if len(self._failed_tests_list) != 0:
            print("Following tests failed: {}".format(self._failed_tests_list))

            blender_path = bpy.app.binary_path
            blend_path = bpy.data.filepath
            frame = inspect.stack()[1]
            module = inspect.getmodule(frame[0])
            python_path = module.__file__

            print("Run following command to open Blender and run the failing test:")
            print("{} {} --python {} -- {} {}"
                  .format(blender_path, blend_path, python_path, "--run-test", "<test_index>"))

            raise Exception("Tests {} failed".format(self._failed_tests_list))
