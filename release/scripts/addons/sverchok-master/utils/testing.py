
import bpy
import os
from os.path import dirname, basename, join
import unittest
import json
from io import StringIO
import logging
from contextlib import contextmanager
import ast

import sverchok
from sverchok.data_structure import get_data_nesting_level
from sverchok.core.socket_data import SvNoDataError, get_output_socket_data
from sverchok.utils.logging import debug, info
from sverchok.utils.context_managers import sv_preferences
from sverchok.utils.sv_IO_panel_tools import import_tree

##########################################
# Utility methods
##########################################

def generate_node_definition(node):
    """
    Generate code that programmatically creates specified node.
    This works only for simple cases.
    """

    result = """
tree = get_or_create_node_tree()
node = create_node("{}", tree.name)
""".format(node.bl_idname)
    
    for k, v in node.items():
        result += "node.{} = {}\n".format(k, v)

    return result

def get_node_editor_context():
    """
    Prepare context override for bpy operators that need context.
    """
    win      = bpy.context.window
    scr      = win.screen
    areas  = [area for area in scr.areas if area.type == 'NODE_EDITOR']
    region   = [region for region in areas[0].regions if region.type == 'WINDOW']

    context = {'window':win,
                'screen':scr,
                'area'  :areas[0],
                'region':region,
                'scene' :bpy.context.scene,
                'space': areas[0].spaces[0]
                }
    return context

def create_node_tree(name=None, must_not_exist=True):
    """
    Create new Sverchok node tree in the scene.
    If must_not_exist == True (default), then it is checked that
    the tree with such name did not exist before. If it exists,
    an exception is raised.
    If must_not_exist == False, then new tree will be created anyway,
    but it can be created with another name (standard Blender's renaming).
    """
    if name is None:
        name = "TestingTree"
    if must_not_exist:
        if name in bpy.data.node_groups:
            raise Exception("Will not create tree `{}': it already exists".format(name))
    debug("Creating tree: %s", name)
    return bpy.data.node_groups.new(name=name, type="SverchCustomTreeType")

def get_or_create_node_tree(name=None):
    """
    Create new Sverchok node tree or reuse existing one.
    """
    if name is None:
        name = "TestingTree"
    if name in bpy.data.node_groups:
        debug("Using existing tree: %s", name)
        return bpy.data.node_groups[name]
    else:
        return create_node_tree(name)

def get_node_tree(name=None):
    """
    Return existing node tree, or raise an exception if there is no such.
    """
    if name is None:
        name = "TestingTree"
    if name in bpy.data.node_groups:
        debug("Using existing tree: %s", name)
        return bpy.data.node_groups[name]
    else:
        raise Exception("There is no node tree named `{}'".format(name))

def remove_node_tree(name=None):
    """
    Remove existing Sverchok node tree.
    """
    if name is None:
        name = "TestingTree"
    if name in bpy.data.node_groups:
        win      = bpy.context.window
        scr      = win.screen
        areas  = [area for area in scr.areas if area.type == 'NODE_EDITOR']
        if len(areas):
            space = areas[0].spaces[0]
            space.node_tree = None
        debug("Removing tree: %s", name)
        tree = bpy.data.node_groups[name]
        bpy.data.node_groups.remove(tree)

def link_node_tree(reference_blend_path, tree_name=None):
    """
    Link node tree from specified .blend file.
    """
    if tree_name is None:
        tree_name = "TestingTree"
    if tree_name in bpy.data.node_groups:
        raise Exception("Tree named `{}' already exists in current scene".format(tree_name))
    with bpy.data.libraries.load(reference_blend_path, link=True) as (data_src, data_dst):
        data_dst.node_groups = [tree_name]

def link_text_block(reference_blend_path, block_name):
    """
    Link text block from specified .blend file.
    """

    with bpy.data.libraries.load(reference_blend_path, link=True) as (data_src, data_dst):
        data_dst.texts = [block_name]

def create_node(node_type, tree_name=None):
    """
    Create Sverchok node by it's bl_idname.
    """
    if tree_name is None:
        tree_name = "TestingTree"
    debug("Creating node of type %s", node_type)
    return bpy.data.node_groups[tree_name].nodes.new(type=node_type)

def get_node(node_name, tree_name=None):
    """
    Return existing node.
    """
    if tree_name is None:
        tree_name = "TestingTree"
    if tree_name not in bpy.data.node_groups:
        raise Exception("There is no node tree named `{}'".format(tree_name))
    return bpy.data.node_groups[tree_name].nodes[node_name]

def get_tests_path():
    """
    Return path to all test cases (tests/ directory).
    """
    sv_init = sverchok.__file__
    tests_dir = join(dirname(sv_init), "tests")
    return tests_dir

def run_all_tests():
    """
    Run all existing test cases.
    Test cases are looked up under tests/ directory.
    """

    tests_path = get_tests_path()
    log_handler = logging.FileHandler(join(tests_path, "sverchok_tests.log"), mode='w')
    logging.getLogger().addHandler(log_handler)
    try:
        loader = unittest.TestLoader()
        suite = loader.discover(start_dir = tests_path, pattern = "*_tests.py")
        buffer = StringIO()
        runner = unittest.TextTestRunner(stream = buffer, verbosity=2)
        result = runner.run(suite)
        info("Test cases result:\n%s", buffer.getvalue())
        return result
    finally:
        logging.getLogger().removeHandler(log_handler)

##############################################
# Base test case classes
##############################################

class SverchokTestCase(unittest.TestCase):
    """
    Base class for Sverchok test cases.
    """

    @contextmanager
    def temporary_node_tree(self, new_tree_name):
        """
        Context manager for dealing with new temporary node tree.
        The tree is created on entering context and removed when
        exiting context. Example of usage:

        with self.temporary_node_tree("TempTree") as tmp:
            do_something(tree)
        """
        new_tree = create_node_tree(new_tree_name)
        try:
            yield new_tree
        finally:
            remove_node_tree(new_tree_name)

    def serialize_json(self, data):
        """
        Serialize JSON object in standard format.
        """
        return json.dumps(data, sort_keys=True, indent=2)

    def store_reference_json(self, file_name, json_data):
        """
        Store JSON data for further reference.
        """
        with open(self.get_reference_file_path(file_name), 'wb') as f:
            data = json.dumps(json_data).encode('utf8')
            f.write(data)

    def get_reference_file_path(self, file_name):
        return join(get_tests_path(), "references", file_name)

    def load_reference_sverchok_data(self, file_name):
        """
        Load reference data in Sverchok format
        (plain Python syntax of nested lists).
        Returns: Sverchok data (nested lists).
        """
        with open(self.get_reference_file_path(file_name), 'r') as f:
            data = f.read()
            return ast.literal_eval(data)

    def assert_json_equals(self, actual_json, expected_json):
        """
        Assert that two JSON objects are equal.
        Comparasion is done by serializing both objects.
        """
        actual_data = self.serialize_json(actual_json)
        expected_data = self.serialize_json(expected_json)
        self.assertEquals(actual_data, expected_data)

    def assert_json_equals_file(self, actual_json, expected_json_file_name):
        """
        Assert that actual_json equals to JSON stored in expected_json_file_name.
        """
        with open(self.get_reference_file_path(expected_json_file_name), 'rb') as f:
            data = f.read().decode('utf8')
            expected_result = json.loads(data)
            self.assert_json_equals(actual_json, expected_result)

    def assert_node_property_equals(self, tree_name, node_name, property_name, expected_value):
        """
        Assert that named property of the node equals to specified value.
        """
        node = get_node(node_name, tree_name)
        actual_value = getattr(node, property_name)
        self.assertEqual(actual_value, expected_value)

    def assert_nodes_linked(self, tree_name, node1_name, node1_output_name, node2_name, node2_input_name):
        """
        Assert that certain output of node1 is linked to certain input of node2.
        """
        node1 = get_node(node1_name, tree_name)
        node2 = get_node(node2_name, tree_name)

        if node1_output_name not in node1.outputs:
            raise AssertionError("Node `{}' does not have output named `{}'".format(node1_name, node1_output_name))
        if node2_input_name not in node2.inputs:
            raise AssertionError("Node `{}' does not have input named `{}'".format(node2_name, node2_input_name))

        if not node1.outputs[node1_output_name].is_linked:
            raise AssertionError("Output `{}' of node `{}' is not linked to anything", node1_output_name, node1_name)
        if not node2.inputs[node2_input_name].is_linked:
            raise AssertionError("Input `{}' of node `{}' is not linked to anything", node2_input_name, node2_name)

        self.assertEquals(node1.outputs[node1_output_name].other, node2.inputs[node2_input_name])

    def assert_nodes_are_equal(self, actual, reference):
        """
        Assert that two nodes have the same settings.
        This works only for simple nodes.
        """
        if actual.bl_idname != reference.bl_idname:
            raise AssertionError("Actual node {} has bl_idname `{}', but reference has `{}'".format(actual, actual.bl_idname, reference.bl_idname))
        for k, v in actual.items():
            if k not in reference:
                raise AssertionError("Property `{}' is present is actual node {}, but is not present in reference".format(k, actual))
            if v != reference[k]:
                raise AssertionError("Property `{}' has value `{}' in actual node {}, but in reference it has value `{}'".format(k, v, actual, reference[k]))

        for k in reference.keys():
            if k not in actual:
                raise AssertionError("Property `{}' is present in reference node, but is not present in actual node {}".format(k, actual))

    def assert_node_equals_file(self, actual_node, reference_node_name, reference_file_name, imported_tree_name=None):
        """
        Assert that actual_node equals to node named reference_node_name imported from file reference_file_name.
        This works only for simple nodes.
        """
        if imported_tree_name is None:
            imported_tree_name = "ImportedTree"

        try:
            new_tree = get_or_create_node_tree(imported_tree_name)
            import_tree(new_tree, self.get_reference_file_path(reference_file_name))
            self.assert_nodes_are_equal(actual_node, get_node(reference_node_name, imported_tree_name))
        finally:
            remove_node_tree(imported_tree_name)

    def assert_numpy_arrays_equal(self, arr1, arr2, precision=None):
        """
        Assert that two numpy arrays are equal.
        Floating-point numbers are compared with specified precision.
        """
        if arr1.shape != arr2.shape:
            raise AssertionError("Shape of 1st array {} != shape of 2nd array {}".format(arr1.shape, arr2.shape))
        shape = list(arr1.shape)

        def compare(prev_indicies):
            step = len(prev_indicies) 
            if step == arr1.ndim:
                ind = tuple(prev_indicies)
                if precision is None:
                    a1 = arr1[ind]
                    a2 = arr2[ind]
                else:
                    a1 = round(arr1[ind], precision)
                    a2 = round(arr2[ind], precision)

                self.assertEqual(a1, a2, "Array 1 [{}] != Array 2 [{}]".format(ind, ind))
            else:
                for idx in range(shape[step]):
                    new_indicies = prev_indicies[:]
                    new_indicies.append(idx)
                    compare(new_indicies)

        compare([])

    def assert_sverchok_data_equal(self, data1, data2, precision=None):
        """
        Assert that two arrays of Sverchok data (nested tuples or lists)
        are equal.
        Floating-point numbers are compared with specified precision.
        """
        level1 = get_data_nesting_level(data1)
        level2 = get_data_nesting_level(data2)
        if level1 != level2:
            raise AssertionError("Nesting level of 1st data {} != nesting level of 2nd data {}".format(level1, level2))
        
        def do_assert(d1, d2, idxs):
            if precision is not None:
                d1 = round(d1, precision)
                d2 = round(d2, precision)
            self.assertEqual(d1, d2, "Data 1 [{}] != Data 2 [{}]".format(idxs, idxs))

        if level1 == 0:
            do_assert(data1, data2, [])
            return

        def compare(prev_indicies, item1, item2):
            step = len(prev_indicies)
            index = prev_indicies[-1]
            if step == level1:
                if index >= len(item1):
                    raise AssertionError("At {}: index {} >= length of Item 1: {}".format(prev_indicies, index, item1))
                if index >= len(item2):
                    raise AssertionError("At {}: index {} >= length of Item 2: {}".format(prev_indicies, index, item2))
                do_assert(item1[index], item2[index], prev_indicies)
            else:
                l1 = len(item1)
                l2 = len(item2)
                self.assertEquals(l1, l2, "Size of data 1 at level {} != size of data 2".format(step))
                for next_idx in range(len(item1[index])):
                    new_indicies = prev_indicies[:]
                    new_indicies.append(next_idx)
                    compare(new_indicies, item1[index], item2[index])

        for idx in range(len(data1)):
            compare([idx], data1, data2)

    def assert_sverchok_data_equals_file(self, data, expected_data_file_name, precision=None):
        expected_data = self.load_reference_sverchok_data(expected_data_file_name)
        #info("Data: %s", data)
        #info("Expected data: %s", expected_data)
        self.assert_sverchok_data_equal(data, expected_data, precision=precision)
        #self.assertEquals(data, expected_data)

    def subtest_assert_equals(self, value1, value2, message=None):
        """
        The same as assertEquals(), but within subtest.
        Use this to do several assertions per test method,
        for case test execution not to be stopped at
        the first failure.
        """

        with self.subTest():
            self.assertEquals(value1, value2, message)


class EmptyTreeTestCase(SverchokTestCase):
    """
    Base class for test cases, that work on empty node tree.
    At setup, it creates new node tree (it becomes available as self.tree).
    At teardown, it removes created node tree.
    """

    def setUp(self):
        self.tree = get_or_create_node_tree()

    def tearDown(self):
        remove_node_tree()

class ReferenceTreeTestCase(SverchokTestCase):
    """
    Base class for test cases, that require existing node tree
    for their work.
    At setup, this class links a node tree from specified .blend
    file into current scene. Name of .blend (or better .blend.gz)
    file must be specified in `reference_file_name` property
    of inherited class. Name of linked tree can be specified
    in `reference_tree_name' property, by default it is "TestingTree".
    The linked node tree is available as `self.tree'.
    At teardown, this class removes that tree from scene.
    """

    reference_file_name = None
    reference_tree_name = None

    def get_reference_file_path(self, file_name=None):
        if file_name is None:
            file_name = self.reference_file_name
        return join(get_tests_path(), "references", file_name)

    def link_node_tree(self, tree_name=None):
        if tree_name is None:
            tree_name = self.reference_tree_name
        path = self.get_reference_file_path()
        link_node_tree(path, tree_name)
        return get_node_tree(tree_name)

    def link_text_block(self, block_name):
        link_text_block(self.get_reference_file_path(), block_name)

    def setUp(self):
        if self.reference_file_name is None:
            raise Exception("ReferenceTreeTestCase subclass must have `reference_file_name' set")
        if self.reference_tree_name is None:
            self.reference_tree_name = "TestingTree"
        
        self.tree = self.link_node_tree()

    def tearDown(self):
        remove_node_tree()

class NodeProcessTestCase(EmptyTreeTestCase):
    """
    Base class for test cases that test process() function
    of one single node.
    At setup, this class creates an empty node tree and one
    node in it. bl_idname of tested node must be specified in
    `node_bl_idname' property of child test case class.
    Optionally, some simple nodes can be created (by default
    a Note node) and connected to some outputs of tested node.
    This is useful for nodes that return from process() if they
    see that nothing is linked to outputs.

    In actual test_xxx() method, the test case should call
    self.node.process(), and after that examine output of the
    node by either self.get_output_data() or self.assert_output_data_equals().

    At teardown, the whole tested node tree is deleted.
    """

    node_bl_idname = None
    connect_output_sockets = None
    output_node_bl_idname = "NoteNode"

    def get_output_data(self, output_name):
        """
        Return data that tested node has written to named output socket.
        Returns None if it hasn't written any data.
        """
        try:
            return get_output_socket_data(self.node, output_name)
        except SvNoDataError:
            return None
    
    def assert_output_data_equals(self, output_name, expected_data, message=None):
        """
        Assert that tested node has written expected_data to
        output socket output_name.
        """
        data = self.get_output_data(output_name)
        self.assertEquals(data, expected_data, message)

    def assert_output_data_equals_file(self, output_name, expected_data_file_name, message=None):
        """
        Assert that tested node has written expected data to
        output socket output_name.
        Expected data is stored in reference file expected_data_file_name.
        """
        data = self.get_output_data(output_name)
        expected_data = self.load_reference_sverchok_data(expected_data_file_name)
        self.assert_sverchok_data_equal(data, expected_data, message)

    def setUp(self):
        super().setUp()

        if self.node_bl_idname is None:
            raise Exception("NodeProcessTestCase subclass must have `node_bl_idname' set")

        self.node = create_node(self.node_bl_idname)

        if self.connect_output_sockets and self.output_node_bl_idname:
            for output_name in self.connect_output_sockets:
                out_node = create_node(self.output_node_bl_idname)
                self.tree.links.new(self.node.outputs[output_name], out_node.inputs[0])

######################################################
# Test running conditionals
######################################################

def is_pull_request():
    """
    Return True if we are running a build for pull-request check on Travis CI.
    """
    pull_request = os.environ.get("TRAVIS_PULL_REQUEST", None)
    return (pull_request is not None and pull_request != "false")

def is_integration_server():
    """
    Return True if we a running inside an integration server (Travis CI) build.
    """
    ci = os.environ.get("CI", None)
    return (ci == "true")

def get_ci_branch():
    """
    If we are running inside an integration server build, return
    the name of git branch which we are checking.
    Otherwise, return None.
    """
    branch = os.environ.get("TRAVIS_BRANCH", None)
    print("Branch:", branch)
    return branch

def make_skip_decorator(condition, message):
    def decorator(func):
        if condition():
            return unittest.skip(message)(func)
        else:
            return func

    return decorator

# Here go decorators used to mark test to be executed only in certain conditions.
# Example usage:
#       
#       @manual_only
#       def test_something(self):
#           # This test will not be running on Travis CI, only in manual mode.
#

pull_requests_only = make_skip_decorator(is_pull_request, "Applies only to PR builds")
skip_pull_requests = make_skip_decorator(lambda: not is_pull_request(), "Does not apply to PR builds")
manual_only = make_skip_decorator(lambda: not is_integration_server(), "Applies for manual builds only")

def branches_only(*branches):
    """
    This test should be only executed for specified branches:

        @branches_only("master")
        def test_something(self):
            ...

    Please note that this applies only for Travis CI builds,
    in manual mode this test will be ran anyway.
    """
    return make_skip_decorator(lambda: get_ci_branch() not in branches, "Does not apply to this branch")

######################################################
# UI operator and panel classes
######################################################

class SvRunTests(bpy.types.Operator):
    """
    Run all tests.
    """

    bl_idname = "node.sv_testing_run_all_tests"
    bl_label = "Run all tests"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        run_all_tests()
        return {'FINISHED'}

class SvDumpNodeDef(bpy.types.Operator):
    """
    Print definition of selected node to stdout.
    This works correctly only for simple cases!
    """

    bl_idname = "node.sv_testing_dump_node_def"
    bl_label = "Dump node definition"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        ntree = context.space_data.node_tree
        selection = list(filter(lambda n: n.select, ntree.nodes))
        if len(selection) != 1:
            self.report({'ERROR'}, "Exactly one node must be selected!")
            return {'CANCELLED'}

        node = selection[0]
        print(generate_node_definition(node))
        self.report({'INFO'}, "See console")

        return {'FINISHED'}

class SvTestingPanel(bpy.types.Panel):
    bl_idname = "SvTestingPanel"
    bl_label = "SV Testing"
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = 'Sverchok'
    use_pin = True

    @classmethod
    def poll(cls, context):
        try:
            if context.space_data.edit_tree.bl_idname != 'SverchCustomTreeType':
                return False
            with sv_preferences() as prefs:
                return prefs.developer_mode
        except:
            return False

    def draw(self, context):
        layout = self.layout
        layout.operator("node.sv_testing_run_all_tests")
        layout.operator("node.sv_testing_dump_node_def")

classes = [SvRunTests, SvDumpNodeDef, SvTestingPanel]

def register():
    for clazz in classes:
        bpy.utils.register_class(clazz)

def unregister():
    for clazz in reversed(classes):
        bpy.utils.unregister_class(clazz)

if __name__ == "__main__":
    import sys
    try:
        register()
        result = run_all_tests()
        if not result.wasSuccessful():
            # We have to raise an exception for Blender to exit with specified exit code.
            raise Exception("Some tests failed")
        sys.exit(0)
    except Exception as e:
        print(e)
        sys.exit(1)

