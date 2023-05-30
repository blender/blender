# SPDX-License-Identifier: GPL-2.0-or-later

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
# the abstract class MeshTest.
#
# If the environment variable BLENDER_TEST_UPDATE is set to 1, the <expected_object>
# is updated with the new test result.
# Tests are verbose when the environment variable BLENDER_VERBOSE is set.


from abc import ABC, abstractmethod
import bpy
import bmesh
import functools
import inspect
import os

# Output from this module and from blender itself will occur during tests.
# We need to flush python so that the output is properly interleaved, otherwise
# blender's output for one test will end up showing in the middle of another test...
print = functools.partial(print, flush=True)


class ModifierSpec:
    """
    Holds a Generate or Deform or Physics modifier type and its parameters.
    """

    def __init__(self, modifier_name: str, modifier_type: str, modifier_parameters: dict, frame_end=0):
        """
        Constructs a modifier spec.

        :arg modifier_name: str - name of object modifier, e.g. "myFirstSubsurfModif"
        :arg modifier_type: str - type of object modifier, e.g. "SUBSURF"
        :arg modifier_parameters: dict - {name : val} dictionary giving modifier parameters, e.g. {"quality" : 4}
        :arg frame_end: int - frame at which simulation needs to be baked or modifier needs to be applied.
        """
        self.modifier_name = modifier_name
        self.modifier_type = modifier_type
        self.modifier_parameters = modifier_parameters
        self.frame_end = frame_end

    def __str__(self):
        return "Modifier: " + self.modifier_name + " of type " + self.modifier_type + \
               " with parameters: " + str(self.modifier_parameters)


class ParticleSystemSpec:
    """
    Holds a Particle System modifier and its parameters.
    """

    def __init__(self, modifier_name: str, modifier_type: str, modifier_parameters: dict, frame_end: int):
        """
        Constructs a particle system spec.

        :arg modifier_name: str - name of object modifier, e.g. "Particles"
        :arg modifier_type: str - type of object modifier, e.g. "PARTICLE_SYSTEM"
        :arg modifier_parameters: dict - {name : val} dictionary giving modifier parameters, e.g. {"seed" : 1}
        :arg frame_end: int - the last frame of the simulation at which the modifier is applied
        """
        self.modifier_name = modifier_name
        self.modifier_type = modifier_type
        self.modifier_parameters = modifier_parameters
        self.frame_end = frame_end

    def __str__(self):
        return "Physics Modifier: " + self.modifier_name + " of type " + self.modifier_type + \
               " with parameters: " + str(self.modifier_parameters) + " with frame end: " + str(self.frame_end)


class OperatorSpecEditMode:
    """
    Holds one operator and its parameters.
    """

    def __init__(
            self,
            operator_name: str,
            operator_parameters: dict,
            select_mode: str,
            selection,
            *,
            select_history: bool = False,
    ):
        """
        Constructs an OperatorSpecEditMode. Raises ValueError if selec_mode is invalid.

        :arg operator_name: str - name of mesh operator from bpy.ops.mesh, e.g. "bevel" or "fill"
        :arg operator_parameters: dict - {name : val} dictionary containing operator parameters.
        :arg select_mode: str - mesh selection mode, must be either 'VERT', 'EDGE' or 'FACE'
        :arg selection: sequence - vertices/edges/faces indices to select, e.g. [0, 9, 10].
        :arg: select_history: bool - load selection into bmesh selection history.
        """
        self.operator_name = operator_name
        self.operator_parameters = operator_parameters
        if select_mode not in {'VERT', 'EDGE', 'FACE'}:
            raise ValueError("select_mode must be either {}, {} or {}".format('VERT', 'EDGE', 'FACE'))
        self.select_mode = select_mode
        self.selection = selection
        self.select_history = select_history

    def __str__(self):
        return "Operator: " + self.operator_name + " with parameters: " + str(self.operator_parameters) + \
               " in selection mode: " + self.select_mode + ", selecting " + str(self.selection) + \
               ("and loading bmesh selection history" if (self.select_history) else "")


class OperatorSpecObjectMode:
    """
    Holds an object operator and its parameters. Helper class for DeformModifierSpec.
    Needed to support operations in Object Mode and not Edit Mode which is supported by OperatorSpecEditMode.
    """

    def __init__(self, operator_name: str, operator_parameters: dict):
        """
        :arg operator_name: str - name of the object operator from bpy.ops.object, e.g. "shade_smooth" or "shape_keys"
        :arg operator_parameters: dict - contains operator parameters.
        """
        self.operator_name = operator_name
        self.operator_parameters = operator_parameters

    def __str__(self):
        return "Operator: " + self.operator_name + " with parameters: " + str(self.operator_parameters)


class DeformModifierSpec:
    """
    Holds a list of deform modifier and OperatorSpecObjectMode.
    For deform modifiers which have an object operator
    """

    def __init__(self, frame_number: int, modifier_list: list, object_operator_spec: OperatorSpecObjectMode = None):
        """
        Constructs a Deform Modifier spec (for user input).

        :arg frame_number: int - the frame at which animated keyframe is inserted
        :arg modifier_list: ModifierSpec - contains modifiers
        :arg object_operator_spec: OperatorSpecObjectMode - contains object operators
        """
        self.frame_number = frame_number
        self.modifier_list = modifier_list
        self.object_operator_spec = object_operator_spec

    def __str__(self):
        return "Modifier: " + str(self.modifier_list) + " with object operator " + str(self.object_operator_spec)


class MeshTest(ABC):
    """
    A mesh testing Abstract class that hold common functionalities for testting operations.
    """

    def __init__(self, test_object_name, exp_object_name, test_name=None, threshold=None, do_compare=True):
        """
        :arg test_object_name: str - Name of object of mesh type to run the operations on.
        :arg exp_object_name: str - Name of object of mesh type that has the expected
                                geometry after running the operations.
        :arg test_name: str - Name of the test.
        :arg threshold: exponent: To allow variations and accept difference to a certain degree.
        :arg do_compare: bool - True if we want to compare the test and expected objects, False otherwise.
        """
        self.test_object_name = test_object_name
        self.exp_object_name = exp_object_name
        if test_name:
            self.test_name = test_name
        else:
            filepath = bpy.data.filepath
            self.test_name = bpy.path.display_name_from_filepath(filepath)
        self.threshold = threshold
        self.do_compare = do_compare
        self.update = os.getenv("BLENDER_TEST_UPDATE") is not None
        self.verbose = os.getenv("BLENDER_VERBOSE") is not None
        self.test_updated_counter = 0
        objects = bpy.data.objects
        self.evaluated_object = None
        self.test_object = objects[self.test_object_name]

        if self.update:
            if exp_object_name in objects:
                self.expected_object = objects[self.exp_object_name]
            else:
                self.create_expected_object()
        else:
            self.expected_object = objects[self.exp_object_name]

    def create_expected_object(self):
        """
        Creates an expected object 10 units away
        in Y direction from test object.
        """
        if self.verbose:
            print("Creating expected object...")
        self.create_evaluated_object()
        self.expected_object = self.evaluated_object
        self.expected_object.name = self.exp_object_name
        x, y, z = self.test_object.location
        self.expected_object.location = (x, y + 10, z)
        bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath)

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
        self.apply_operations(self.evaluated_object.name)

        if not self.do_compare:
            print("\nVisualization purpose only: Open Blender in GUI mode")
            print("Compare evaluated and expected object in Blender.\n")
            return False

        result = self.compare_meshes(self.evaluated_object, self.expected_object, self.threshold)

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

        elif self.update:
            self.print_failed_test_result(result)
            self.update_failed_test()
            # Check for testing the blend file is updated and re-running.
            # Also safety check to avoid infinite recursion loop.
            if self.test_updated_counter == 1:
                print("Re-running test...")
                self.run_test()
            else:
                print("The test fails consistently. Exiting...")
                return False

        else:
            self.print_failed_test_result(result)
            return False

    def print_failed_test_result(self, result):
        """
        Print results for failed test.
        """
        print("FAILED {} test with the following: ".format(self.test_name))
        self._print_result(result)

    def print_passed_test_result(self, result):
        """
        Print results for passing test.
        """
        print("PASSED {} test successfully.".format(self.test_name))
        self._print_result(result)

    def do_selection(self, mesh: bpy.types.Mesh, select_mode: str, selection, select_history: bool):
        """
        Do selection on a mesh.

        :arg mesh: bpy.types.Mesh - input mesh
        :arg: select_mode: str - selection mode. Must be 'VERT', 'EDGE' or 'FACE'
        :arg: selection: sequence - indices of selection.
        :arg: select_history: bool - load selection into bmesh selection history

        Example: select_mode='VERT' and selection={1,2,3} selects veritces 1, 2 and 3 of input mesh
        """
        if select_history and isinstance(selection, set):
            raise Exception("'selection' must be an ordered sequence, not a 'set' type when 'select_history=True'")

        # Deselect all objects.
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='DESELECT')

        bm = bmesh.from_edit_mesh(mesh)

        # bpy.ops.object.mode_set(mode='OBJECT')

        bpy.context.tool_settings.mesh_select_mode = (select_mode == 'VERT',
                                                      select_mode == 'EDGE',
                                                      select_mode == 'FACE')

        items = (
            bm.verts if select_mode == 'VERT' else
            bm.edges if select_mode == 'EDGE' else
            bm.faces if select_mode == 'FACE' else None
        )

        items.ensure_lookup_table()

        if items is None:
            raise ValueError("Invalid selection mode")
        for index in selection:
            items[index].select = True

        if select_history:
            for index in selection:
                bm.select_history.add(items[index])
            bm.select_history.validate()

        bpy.ops.object.mode_set(mode='OBJECT')

    def update_failed_test(self):
        """
        Updates expected object.
        """
        self.evaluated_object.location = self.expected_object.location
        expected_object_name = self.expected_object.name
        evaluated_selection = {
            v.index for v in self.evaluated_object.data.vertices if v.select}

        bpy.data.objects.remove(self.expected_object, do_unlink=True)
        self.evaluated_object.name = expected_object_name
        self.do_selection(self.evaluated_object.data, "VERT", evaluated_selection, False)

        # Save file.
        bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath)
        self.test_updated_counter += 1
        self.expected_object = self.evaluated_object

    @staticmethod
    def compare_meshes(evaluated_object, expected_object, threshold):
        """
        Compares evaluated object mesh with expected object mesh.

        :arg evaluated_object: first object for comparison.
        :arg expected_object: second object for comparison.
        :arg threshold: exponent: To allow variations and accept difference to a certain degree.
        :return: dict: Contains results of different comparisons.
        """
        objects = bpy.data.objects
        evaluated_test_mesh = objects[evaluated_object.name].data
        expected_mesh = expected_object.data
        result_codes = {}

        # Mesh Comparison.
        if threshold:
            result_mesh = expected_mesh.unit_test_compare(
                mesh=evaluated_test_mesh, threshold=threshold)
        else:
            result_mesh = expected_mesh.unit_test_compare(
                mesh=evaluated_test_mesh)

        if result_mesh == "Same":
            result_codes['Mesh Comparison'] = (True, result_mesh)
        else:
            result_codes['Mesh Comparison'] = (False, result_mesh)

        # Selection comparison.

        selected_evaluated_verts = [
            v.index for v in evaluated_test_mesh.vertices if v.select]
        selected_expected_verts = [
            v.index for v in expected_mesh.vertices if v.select]

        if selected_evaluated_verts == selected_expected_verts:
            result_selection = "Same"
            result_codes['Selection Comparison'] = (True, result_selection)
        else:
            result_selection = "Selection doesn't match."
            result_codes['Selection Comparison'] = (False, result_selection)

        # Validation check.
        result_validation = evaluated_test_mesh.validate(verbose=True)
        if result_validation:
            result_validation = "Invalid Mesh"
            result_codes['Mesh Validation'] = (False, result_validation)
        else:
            result_validation = "Valid"
            result_codes['Mesh Validation'] = (True, result_validation)

        return result_codes

    @abstractmethod
    def apply_operations(self, object_name):
        """
        Apply operations on this object.

        object_name (str): Name of the test object on which operations will be applied.
        """
        pass


class SpecMeshTest(MeshTest):
    """
    A mesh testing class inherited from MeshTest class targeted at testing modifiers and operators on a single object.
    It holds a stack of mesh operations, i.e. modifiers or operators. The test is executed using MeshTest's run_test.
    """

    def __init__(self, test_name,
                 test_object_name,
                 exp_object_name,
                 operations_stack=None,
                 apply_modifier=True,
                 threshold=None):
        """
        Constructor for SpecMeshTest.

        :arg test_name: str - Name of the test.
        :arg test_object_name: str - Name of object of mesh type to run the operations on.
        :arg exp_object_name: str - Name of object of mesh type that has the expected
                              geometry after running the operations.
        :arg operations_stack: list - stack holding operations to perform on the test_object.
        :arg apply_modifier: bool - True if we want to apply the modifiers right after adding them to the object.
                                  - True if we want to apply the modifier to list of modifiers, after some operation.
                             This affects operations of type ModifierSpec and DeformModifierSpec.
        """

        super().__init__(test_object_name, exp_object_name, test_name, threshold)
        self.test_name = test_name
        if operations_stack is None:
            self.operations_stack = []
        else:
            self.operations_stack = operations_stack
        self.apply_modifier = apply_modifier

    def apply_operations(self, evaluated_test_object_name):
        # Add modifiers and operators.
        SpecMeshTest.apply_operations.__doc__ = MeshTest.apply_operations.__doc__
        evaluated_test_object = bpy.data.objects[evaluated_test_object_name]
        if self.verbose:
            print("Applying operations...")
        for operation in self.operations_stack:
            if isinstance(operation, ModifierSpec):
                self._add_modifier(evaluated_test_object, operation)
                if self.apply_modifier:
                    self._apply_modifier(
                        evaluated_test_object, operation.modifier_name)

            elif isinstance(operation, OperatorSpecEditMode):
                self._apply_operator_edit_mode(
                    evaluated_test_object, operation)

            elif isinstance(operation, OperatorSpecObjectMode):
                self._apply_operator_object_mode(operation)

            elif isinstance(operation, DeformModifierSpec):
                self._apply_deform_modifier(evaluated_test_object, operation)

            elif isinstance(operation, ParticleSystemSpec):
                self._apply_particle_system(evaluated_test_object, operation)

            else:
                raise ValueError("Expected operation of type {} or {} or {} or {}. Got {}".
                                 format(type(ModifierSpec), type(OperatorSpecEditMode),
                                        type(OperatorSpecObjectMode), type(ParticleSystemSpec), type(operation)))

    def _set_parameters_impl(self, modifier, modifier_parameters, nested_settings_path, modifier_name):
        """
        Doing a depth first traversal of the modifier parameters and setting their values.
        :arg: modifier: Of type modifier, its altered to become a setting in recursion.
        :arg: modifier_parameters : dict or sequence, a simple/nested dictionary of modifier parameters.
        :arg: nested_settings_path : list(stack): helps in tracing path to each node.
        """
        if not isinstance(modifier_parameters, dict):
            param_setting = None
            for i, setting in enumerate(nested_settings_path):

                # We want to set the attribute only when we have reached the last setting.
                # Applying of intermediate settings is meaningless.
                if i == len(nested_settings_path) - 1:
                    setattr(modifier, setting, modifier_parameters)

                elif hasattr(modifier, setting):
                    param_setting = getattr(modifier, setting)
                    # getattr doesn't accept canvas_surfaces["Surface"], but we need to pass it to setattr.
                    if setting == "canvas_surfaces":
                        modifier = param_setting.active
                    else:
                        modifier = param_setting
                else:
                    # Clean up first
                    bpy.ops.object.delete()
                    raise Exception("Modifier '{}' has no parameter named '{}'".
                                    format(modifier_name, setting))

            # It pops the current node before moving on to its sibling.
            nested_settings_path.pop()
            return

        for key in modifier_parameters:
            nested_settings_path.append(key)
            self._set_parameters_impl(
                modifier, modifier_parameters[key], nested_settings_path, modifier_name)

        if nested_settings_path:
            nested_settings_path.pop()

    def set_parameters(self, modifier, modifier_parameters):
        """
        Wrapper for _set_parameters_impl.
        """
        settings = []
        modifier_name = modifier.name
        self._set_parameters_impl(modifier, modifier_parameters, settings, modifier_name)

    def _add_modifier(self, test_object, modifier_spec: ModifierSpec):
        """
        Add modifier to object.

        :arg test_object: bpy.types.Object - Blender object to apply modifier on.
        :arg modifier_spec: ModifierSpec - ModifierSpec object with parameters
        """
        bakers_list = ['CLOTH', 'SOFT_BODY', 'DYNAMIC_PAINT', 'FLUID']
        scene = bpy.context.scene
        scene.frame_set(1)
        modifier = test_object.modifiers.new(modifier_spec.modifier_name,
                                             modifier_spec.modifier_type)

        if modifier is None:
            raise Exception("This modifier type is already added on the Test Object, please remove it and try again.")

        if self.verbose:
            print("Created modifier '{}' of type '{}'.".
                  format(modifier_spec.modifier_name, modifier_spec.modifier_type))

        # Special case for Dynamic Paint, need to toggle Canvas on.
        if modifier.type == "DYNAMIC_PAINT":
            bpy.ops.dpaint.type_toggle(type='CANVAS')

        self.set_parameters(modifier, modifier_spec.modifier_parameters)

        if modifier.type in bakers_list:
            self._bake_current_simulation(test_object, modifier.name, modifier_spec.frame_end)

        scene.frame_set(modifier_spec.frame_end)

    def _apply_modifier(self, test_object, modifier_name):
        # Modifier automatically gets applied when converting from Curve to Mesh.
        if test_object.type == 'CURVE':
            bpy.ops.object.convert(target='MESH')
        elif test_object.type == 'MESH':
            bpy.ops.object.modifier_apply(modifier=modifier_name)
        else:
            raise Exception("This object type is not yet supported!")

    def _bake_current_simulation(self, test_object, test_modifier_name, frame_end):
        """
        FLUID: Bakes the simulation
        SOFT BODY, CLOTH, DYNAMIC PAINT: Overrides the point_cache context and then bakes.
        """

        for scene in bpy.data.scenes:
            for modifier in test_object.modifiers:
                if modifier.type == 'FLUID':
                    bpy.ops.fluid.bake_all()
                    break

                elif modifier.type == 'CLOTH' or modifier.type == 'SOFT_BODY':
                    test_object.modifiers[test_modifier_name].point_cache.frame_end = frame_end
                    override_setting = modifier.point_cache
                    override = {'scene': scene, 'active_object': test_object, 'point_cache': override_setting}
                    bpy.ops.ptcache.bake(override, bake=True)
                    break

                elif modifier.type == 'DYNAMIC_PAINT':
                    dynamic_paint_setting = modifier.canvas_settings.canvas_surfaces.active
                    override_setting = dynamic_paint_setting.point_cache
                    override = {'scene': scene, 'active_object': test_object, 'point_cache': override_setting}
                    bpy.ops.ptcache.bake(override, bake=True)
                    break

    def _apply_particle_system(self, test_object, particle_sys_spec: ParticleSystemSpec):
        """
        Applies Particle System settings to test objects
        """
        bpy.context.scene.frame_set(1)
        bpy.ops.object.select_all(action='DESELECT')

        test_object.modifiers.new(particle_sys_spec.modifier_name, particle_sys_spec.modifier_type)

        settings_name = test_object.particle_systems.active.settings.name
        particle_setting = bpy.data.particles[settings_name]
        if self.verbose:
            print("Created modifier '{}' of type '{}'.".
                  format(particle_sys_spec.modifier_name, particle_sys_spec.modifier_type))

        for param_name in particle_sys_spec.modifier_parameters:
            try:
                if param_name == "seed":
                    system_setting = test_object.particle_systems[particle_sys_spec.modifier_name]
                    setattr(system_setting, param_name, particle_sys_spec.modifier_parameters[param_name])
                else:
                    setattr(particle_setting, param_name, particle_sys_spec.modifier_parameters[param_name])

                if self.verbose:
                    print("\t set parameter '{}' with value '{}'".
                          format(param_name, particle_sys_spec.modifier_parameters[param_name]))
            except AttributeError:
                # Clean up first
                bpy.ops.object.delete()
                raise AttributeError("Modifier '{}' has no parameter named '{}'".
                                     format(particle_sys_spec.modifier_type, param_name))

        bpy.context.scene.frame_set(particle_sys_spec.frame_end)
        test_object.select_set(True)
        bpy.ops.object.duplicates_make_real()
        test_object.select_set(True)
        bpy.ops.object.join()
        if self.apply_modifier:
            self._apply_modifier(test_object, particle_sys_spec.modifier_name)

    def _apply_operator_edit_mode(self, test_object, operator: OperatorSpecEditMode):
        """
        Apply operator on test object.

        :arg test_object: bpy.types.Object - Blender object to apply operator on.
        :arg operator: OperatorSpecEditMode - OperatorSpecEditMode object with parameters.
        """
        self.do_selection(
            test_object.data,
            operator.select_mode,
            operator.selection,
            select_history=operator.select_history,
        )

        # Apply operator in edit mode.
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_mode(type=operator.select_mode)
        mesh_operator = getattr(bpy.ops.mesh, operator.operator_name)

        try:
            retval = mesh_operator(**operator.operator_parameters)
        except AttributeError:
            raise AttributeError("bpy.ops.mesh has no attribute {}".format(operator.operator_name))
        except TypeError as ex:
            raise TypeError("Incorrect operator parameters {!r} raised {!r}".format(operator.operator_parameters, ex))

        if retval != {'FINISHED'}:
            raise RuntimeError("Unexpected operator return value: {}".format(operator.operator_name))
        if self.verbose:
            print("Applied {}".format(operator))

        bpy.ops.object.mode_set(mode='OBJECT')

    def _apply_operator_object_mode(self, operator: OperatorSpecObjectMode):
        """
        Applies the object operator.
        """
        bpy.ops.object.mode_set(mode='OBJECT')
        object_operator = getattr(bpy.ops.object, operator.operator_name)

        try:
            retval = object_operator(**operator.operator_parameters)
        except AttributeError:
            raise AttributeError("bpy.ops.object has no attribute {}".format(operator.operator_name))
        except TypeError as ex:
            raise TypeError("Incorrect operator parameters {!r} raised {!r}".format(operator.operator_parameters, ex))

        if retval != {'FINISHED'}:
            raise RuntimeError("Unexpected operator return value: {}".format(retval))
        if self.verbose:
            print("Applied operator {}".format(operator))

    def _apply_deform_modifier(self, test_object, operation: list):
        """
        arg: operation: list: List of modifiers or combination of modifier and object operator.
        """

        scene = bpy.context.scene
        scene.frame_set(1)
        bpy.ops.object.mode_set(mode='OBJECT')
        modifier_operations_list = operation.modifier_list
        modifier_names = []
        object_operations = operation.object_operator_spec
        for modifier_operations in modifier_operations_list:
            if isinstance(modifier_operations, ModifierSpec):
                self._add_modifier(test_object, modifier_operations)
                modifier_names.append(modifier_operations.modifier_name)

        if isinstance(object_operations, OperatorSpecObjectMode):
            self._apply_operator_object_mode(object_operations)

        scene.frame_set(operation.frame_number)

        if self.apply_modifier:
            for mod_name in modifier_names:
                self._apply_modifier(test_object, mod_name)


class BlendFileTest(MeshTest):
    """
    A mesh testing class inherited from MeshTest aimed at testing operations like modifiers loaded directly from
    blend file i.e. without adding them from scratch or without adding specifications.
    """

    def apply_operations(self, evaluated_test_object_name):

        BlendFileTest.apply_operations.__doc__ = MeshTest.apply_operations.__doc__
        evaluated_test_object = bpy.data.objects[evaluated_test_object_name]
        modifiers_list = evaluated_test_object.modifiers
        if not modifiers_list:
            raise Exception("No modifiers are added to test object.")
        for modifier in modifiers_list:
            bpy.ops.object.modifier_apply(modifier=modifier.name)


class RunTest:
    """
    Helper class that stores and executes SpecMeshTest tests.

    Example usage:

    >>> modifier_list = [
    >>>     ModifierSpec("firstSUBSURF", "SUBSURF", {"quality": 5}),
    >>>     ModifierSpec("firstSOLIDIFY", "SOLIDIFY", {"thickness_clamp": 0.9, "thickness": 1})
    >>> ]
    >>> operator_list = [
    >>>     OperatorSpecEditMode("delete_edgeloop", {}, "EDGE", MONKEY_LOOP_EDGE),
    >>> ]
    >>> tests = [
    >>>     SpecMeshTest("Test1", "testCube", "expectedCube", modifier_list),
    >>>     SpecMeshTest("Test2", "testCube_2", "expectedCube_2", modifier_list),
    >>>     SpecMeshTest("MonkeyDeleteEdge", "testMonkey","expectedMonkey", operator_list)
    >>> ]
    >>> modifiers_test = RunTest(tests)
    >>> modifiers_test.run_all_tests()
    """

    def __init__(self, tests, apply_modifiers=False, do_compare=False):
        """
        Construct a test suite.

        :arg tests: list - list of modifier or operator test cases. Each element in the list must contain the
        following in the correct order:
             0) test_name: str - unique test name
             1) test_object_name: bpy.Types.Object - test object
             2) expected_object_name: bpy.Types.Object - expected object
             3) modifiers or operators: list - list of mesh_test.ModifierSpec objects or
             mesh_test.OperatorSpecEditMode objects
        """
        self.tests = tests
        self._ensure_unique_test_name_or_raise_error()
        self.apply_modifiers = apply_modifiers
        self.do_compare = do_compare
        self.verbose = os.environ.get("BLENDER_VERBOSE") is not None
        self._failed_tests_list = []

    def _ensure_unique_test_name_or_raise_error(self):
        """
        Check if the test name is unique else raise an error.
        """
        all_test_names = []
        for each_test in self.tests:
            test_name = each_test.test_name
            all_test_names.append(test_name)

        seen_name = set()
        for ele in all_test_names:
            if ele in seen_name:
                raise ValueError("{} is a duplicate, write a new unique name.".format(ele))
            else:
                seen_name.add(ele)

    def run_all_tests(self):
        """
        Run all tests in self.tests list. Displays all failed tests at bottom.
        """
        for test_number, each_test in enumerate(self.tests):
            test_name = each_test.test_name
            if self.verbose:
                print()
                print("Running test {}/{}: {}...".format(test_number + 1, len(self.tests), test_name))
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
        test.apply_modifier = self.apply_modifiers
        test.do_compare = self.do_compare

        success = test.run_test()
        return success
