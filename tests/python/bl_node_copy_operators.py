# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys
import tempfile
import unittest
from mathutils import Vector

import bpy

# This test is based around the "Tests" node group in the test file.
# Test cases are added by frame nodes in the node group. Each frame represents one sub-test.
# A single test case can be tested using the '--subtest <NAME>' argument:
#
# ./bin/blender --factory-startup --python <SOURCEPATH>/tests/python/bl_node_copy_operators.py
#     --
#     --testdir <SOURCEPATH>/tests/files/node_group --subtest <NAME>
#
# Operators are applied to each frame and compared to expected results,
# stored in separate node trees:
#   - bpy.data.node_groups["ExpectedMakeGroup"]:            Result of node.make_group operator.
#   - bpy.data.node_groups["ExpectedGroupInsert"]:          Result of node.group_insert operator.
#   - bpy.data.node_groups["ExpectedUngroup"]:              Result of node.ungroup operator.
#   - bpy.data.node_groups["ExpectedGroupSeparateCopy"]:    Result of node.group_separate operator with type='COPY'.
#   - bpy.data.node_groups["ExpectedGroupSeparateMove"]:    Result of node.group_separate operator with type='MOVE'.
#
# The script can be invoked with an additional argument '--generate' to update the ground truth test data.
# Nodes in the "Expected***" node trees are replaced with the result of operators applied to the "Tests" node tree.
#
# ./bin/blender --factory-startup --python <SOURCEPATH>/tests/python/bl_node_copy_operators.py
#     --
#     --testdir <SOURCEPATH>/tests/files/node_group --generate

args = None
testfile = "node_copy_operators.blend"


# Utility for mapping nodes and sockets to ground truth data.
class NodeMapping:
    def __init__(self):
        self.tree_map = dict()
        self.node_map = dict()
        self.socket_map = dict()

    def add_tree(self, test_tree, expected_tree):
        self.tree_map[test_tree] = expected_tree

    def add_node(self, test_node, expected_node):
        self.node_map[test_node] = expected_node
        # Add all sockets of mapped nodes to their own dictionary, assuming the socket order is the same.
        for test_socket, expected_socket in zip(test_node.inputs, expected_node.inputs):
            self.socket_map[test_socket] = expected_socket
        for test_socket, expected_socket in zip(test_node.outputs, expected_node.outputs):
            self.socket_map[test_socket] = expected_socket

    def add_nodes_by_name(self, test_nodes, expected_nodes):
        expected_nodes_map = {node.name: node for node in expected_nodes}
        for test_node in test_nodes:
            # Raises key error if not all test nodes can be mapped.
            expected_node = expected_nodes_map.pop(test_node.name)
            self.add_node(test_node, expected_node)
            self.add_node(test_node, expected_node)
        # Should map all expected nodes.
        assert not expected_nodes_map


def open_test_file():
    bpy.ops.wm.open_mainfile(filepath=str(args.testdir / testfile))


def save_test_file():
    bpy.ops.wm.save_mainfile(filepath=str(args.testdir / testfile))


def select_nodes(tree, selected_nodes, active_node=None):
    for node in tree.nodes:
        node.select = False
    for node in selected_nodes:
        node.select = True
    tree.nodes.active = active_node if active_node else (selected_nodes[0] if selected_nodes else None)


def node_centroid(nodes):
    return sum((node.location for node in nodes), Vector((0.0, 0.0))) / max(len(nodes), 1)


# Provide a valid context override to run node editor operators
def node_editor_context_override(context, tree, selected_nodes=[], active_node=None):
    window = context.window if context.window else next(
        window for window in context.window_manager.windows if window.screen is not None)
    screen = context.screen if context.screen else window.screen
    area = next(area for area in screen.areas if area.type == 'NODE_EDITOR')
    region = next(region for region in area.regions if region.type == 'WINDOW')
    space = area.spaces[0]

    # Explicitly set the space tree, otherwise requires a context update to ensure
    # that the space tree matches the active modifier tree.
    space.node_tree = tree
    # Relying on context.selected_nodes and context.active_node does not work for many/most node operators
    # because they rely on actual selected/active nodes in the tree, rather than the context.
    select_nodes(tree, selected_nodes, active_node)

    context_override = context.copy()
    context_override["window"] = window
    context_override["screen"] = screen
    context_override["area"] = area
    context_override["region"] = region
    context_override["space_data"] = space
    context_override["selected_nodes"] = selected_nodes
    context_override["active_node"] = tree.nodes.active
    return context.temp_override(**context_override)

# Find all top-level frames in a tree.


def top_level_frames(tree):
    for node in tree.nodes:
        if isinstance(node, bpy.types.NodeFrame) and node.parent is None:
            yield node


# Names of test cases found in a node tree.
def test_cases(tree):
    return [frame.label for frame in top_level_frames(tree)]


# Filter test cases based on script arguments.
def filtered_test_cases(tree):
    if args.subtest:
        return filter(lambda test_case: test_case == args.subtest, test_cases(tree))
    else:
        return test_cases(tree)


# Returns the frame node for a test case.
def find_test_frame(tree, test_name):
    for frame in top_level_frames(tree):
        if frame.label == test_name:
            return frame


# Find nodes inside a top level frame of the given name.
def find_expected_nodes(tree, test_name):
    test_nodes = list()
    for node in tree.nodes:
        top_parent = node.parent
        while top_parent:
            if not top_parent.parent:
                break
            top_parent = top_parent.parent

        if top_parent and isinstance(top_parent, bpy.types.NodeFrame) and top_parent.label == test_name:
            test_nodes.append(node)
    return test_nodes


# Run the 'node.make_group' operator on test nodes.
def execute_make_group(test_case, test_tree, expected_tree=None):
    test_nodes = find_expected_nodes(test_tree, test_case)

    with node_editor_context_override(bpy.context, test_tree, selected_nodes=test_nodes):
        bpy.ops.node.group_make()
    group_node = test_tree.nodes.active
    # Re-attach to the parent frame to identify the operator result.
    group_node.parent = find_test_frame(test_tree, test_case)

    if expected_tree:
        # Map resulting nodes to expected nodes.
        expected_nodes = find_expected_nodes(expected_tree, test_case)
        assert len(expected_nodes) == 1
        expected_node = expected_nodes[0]
        mapping = NodeMapping()
        mapping.add_tree(group_node.node_tree, expected_node.node_tree)
        mapping.add_node(group_node, expected_node)
        mapping.add_nodes_by_name(group_node.node_tree.nodes, expected_node.node_tree.nodes)
        return mapping


# Run the 'node.group_insert' operator on test nodes.
def execute_group_insert(test_case, test_tree, expected_tree=None):
    test_nodes = find_expected_nodes(test_tree, test_case)
    centroid = node_centroid(test_nodes)

    # Make empty node group.
    group_tree = bpy.data.node_groups.new(f"{test_case}_GroupInsert", 'GeometryNodeTree')
    # Copy nodes into the tree to force deduplication testing.
    # Note: this is not ideal since it depends on yet another operator,
    # but there is no way to retain the original nodes when inserting to enforce duplicate names.
    with node_editor_context_override(bpy.context, test_tree, selected_nodes=test_nodes):
        bpy.ops.node.clipboard_copy()
    with node_editor_context_override(bpy.context, group_tree):
        bpy.ops.node.clipboard_paste()
    # Make a group node with the new tree.
    with node_editor_context_override(bpy.context, test_tree):
        bpy.ops.node.add_node(
            settings=[
                {"name": "name", "value": f"'{test_case}_GroupNode'"},
                {"name": "node_tree", "value": f"bpy.data.node_groups['{group_tree.name}']"},
            ],
            type='GeometryNodeGroup',
        )
        group_node = test_tree.nodes.active
        group_node.parent = find_test_frame(test_tree, test_case)
        group_node.location = centroid
    # Insert nodes into the group.
    with node_editor_context_override(bpy.context, test_tree, selected_nodes=test_nodes + [group_node], active_node=group_node):
        bpy.ops.node.group_insert()

    if expected_tree:
        # Map resulting nodes to expected nodes.
        expected_nodes = find_expected_nodes(expected_tree, test_case)
        assert len(expected_nodes) == 1
        expected_node = expected_nodes[0]
        mapping = NodeMapping()
        mapping.add_tree(group_node.node_tree, expected_node.node_tree)
        mapping.add_node(group_node, expected_node)
        mapping.add_nodes_by_name(group_node.node_tree.nodes, expected_node.node_tree.nodes)
        return mapping


# Run the 'node.ungroup' operator on test nodes.
def execute_ungroup(test_case, test_tree, expected_tree=None):
    test_nodes = find_expected_nodes(test_tree, test_case)

    with node_editor_context_override(bpy.context, test_tree, selected_nodes=test_nodes):
        bpy.ops.node.group_ungroup()
    internal_nodes = [node for node in test_tree.nodes if node.select]
    # Re-attach to the parent frame to identify the operator result.
    for node in internal_nodes:
        if node.parent is None:
            node.parent = find_test_frame(test_tree, test_case)

    if expected_tree:
        # Map resulting nodes to expected nodes.
        expected_nodes = find_expected_nodes(expected_tree, test_case)
        mapping = NodeMapping()
        mapping.add_nodes_by_name(internal_nodes, expected_nodes)
        return mapping


# Run the 'node.group_separate' operator on test nodes.
# type can be 'COPY' or 'MOVE'.
def execute_group_separate(type, test_case, test_tree, expected_tree=None):
    test_nodes = find_expected_nodes(test_tree, test_case)

    # Test nodes should be node groups
    assert len(test_nodes) == 1
    group_node = test_nodes[0]
    assert isinstance(group_node, bpy.types.GeometryNodeGroup)

    # Ensure single-user node group, so that moving nodes out does not modify a shared tree.
    group_node.node_tree = group_node.node_tree.copy()

    with node_editor_context_override(bpy.context, test_tree, selected_nodes=[group_node]):
        bpy.ops.node.group_edit(exit=False)
        # Stay in current context so that the tree path has a valid "parent" tree to copy nodes into.

        # Select all nodes for separating.
        select_nodes(group_node.node_tree, selected_nodes=group_node.node_tree.nodes)
        bpy.ops.node.group_separate(type=type)

    separated_nodes = [node for node in test_tree.nodes if node.select and node.parent is None]
    centroid = node_centroid(separated_nodes)
    # Re-attach to the parent frame to identify the operator result.
    for node in separated_nodes:
        offset = node.location - centroid
        node.parent = find_test_frame(test_tree, test_case)
        node.location = group_node.location + Vector((0, -1000)) + offset

    if expected_tree:
        # Map resulting nodes to expected nodes.
        result_nodes = find_expected_nodes(test_tree, test_case)
        expected_nodes = find_expected_nodes(expected_tree, test_case)
        mapping = NodeMapping()
        mapping.add_nodes_by_name(result_nodes, expected_nodes)
        return mapping


class AbstractNodeCopyOperatorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tempdir = tempfile.TemporaryDirectory()
        cls.tempdir = pathlib.Path(cls._tempdir.name)

    def setUp(self):
        self.assertTrue(args.testdir.exists(),
                        'Test dir {0} should exist'.format(args.testdir))
        open_test_file()
        self.assertEqual(bpy.data.version, (5, 1, 16))

    def tearDown(self):
        self._tempdir.cleanup()

    def compare_value(self, bl_idname, value_a, value_b):
        # Note: Socket subtypes are not actually subclasses of the base types.
        # We rely on name prefixes instead of python issubclass tests here, to keep this check compact.
        vector_socket_prefixes = ["NodeSocketColor", "NodeSocketMatrix", "NodeSocketRotation", "NodeSocketVector"]
        if any(bl_idname.startswith(prefix) for prefix in vector_socket_prefixes):
            for comp_a, comp_b in zip(value_a, value_b):
                self.assertEqual(comp_a, comp_b)
        else:
            self.assertEqual(value_a, value_b)

    # Validate node socket properties and connections in the tree.
    # Links to/from the socket are compared to expected values using the node and socket maps.
    def compare_socket(self, test_socket, mapping):
        expected_socket = mapping.socket_map.get(test_socket, None)
        self.assertIsNotNone(expected_socket)
        with self.subTest(test_socket=test_socket.name, expected_socket=expected_socket.name):
            # Generic socket properties
            self.assertEqual(test_socket.name, expected_socket.name)
            self.assertEqual(test_socket.bl_idname, expected_socket.bl_idname)
            self.assertEqual(test_socket.type, expected_socket.type)
            self.assertEqual(test_socket.description, expected_socket.description)
            self.assertEqual(test_socket.is_output, expected_socket.is_output)

            # Input value
            if not expected_socket.is_output:
                self.assertEqual(test_socket.hide_value, expected_socket.hide_value)
                test_has_value = hasattr(test_socket, "default_value")
                expected_has_value = hasattr(expected_socket, "default_value")
                self.assertEqual(test_has_value, expected_has_value)
                if test_has_value and expected_has_value:
                    self.compare_value(
                        expected_socket.bl_idname,
                        test_socket.default_value,
                        expected_socket.default_value)

            # Links
            self.assertEqual(test_socket.is_linked, expected_socket.is_linked)
            if expected_socket.is_linked:
                self.assertEqual(len(test_socket.links), len(expected_socket.links))
                for test_link, expected_link in zip(test_socket.links, expected_socket.links):
                    # If there is no entry in the mapping for the connected test socket yet then the expected socket is use as default.
                    # External connections are not usually added to the map to keep test cases simple.
                    # This ensures that any socket with external links is in fact connected,
                    # without specifying the exact external node.
                    if expected_socket.is_output:
                        self.assertEqual(
                            mapping.node_map.setdefault(
                                test_link.to_node,
                                expected_link.to_node),
                            expected_link.to_node)
                        self.assertEqual(
                            mapping.socket_map.setdefault(
                                test_link.to_socket,
                                expected_link.to_socket),
                            expected_link.to_socket)
                    else:
                        self.assertEqual(
                            mapping.node_map.setdefault(
                                test_link.from_node,
                                expected_link.from_node),
                            expected_link.from_node)
                        self.assertEqual(
                            mapping.socket_map.setdefault(
                                test_link.from_socket,
                                expected_link.from_socket),
                            expected_link.from_socket)

    # Validate a node against the expected data using the node map.
    def compare_node(self, test_node, expected_node, mapping):
        self.assertEqual(len(test_node.inputs), len(expected_node.inputs))
        self.assertEqual(len(test_node.outputs), len(expected_node.outputs))
        for test_socket in test_node.inputs:
            self.compare_socket(test_socket, mapping)
        for test_socket in test_node.outputs:
            self.compare_socket(test_socket, mapping)

    # Validate the tree interface settings of a node group.
    def compare_tree_interface(self, test_tree, expected_tree):
        test_items = test_tree.interface.items_tree
        expected_items = expected_tree.interface.items_tree
        self.assertEqual(len(test_items), len(expected_items))
        for test_item, expected_item in zip(test_items, expected_items):
            self.assertEqual(test_item.index, expected_item.index)
            self.assertEqual(test_item.item_type, expected_item.item_type)
            # Find expected parent panel by index from the expected items list.
            # Item with index -1 is the root panel and can be ignored.
            if test_item.parent.index >= 0:
                expected_parent = expected_items[test_item.parent.index]
                self.assertEqual(expected_parent, expected_item.parent)
            else:
                self.assertEqual(test_item.parent.index, -1)
            self.assertEqual(test_item.position, expected_item.position)

            if expected_item.item_type == 'SOCKET':
                # General properties.
                self.assertEqual(test_item.bl_socket_idname, expected_item.bl_socket_idname)
                self.assertEqual(test_item.in_out, expected_item.in_out)
                self.assertEqual(test_item.name, expected_item.name)
                self.assertEqual(test_item.description, expected_item.description)
                self.assertEqual(test_item.optional_label, expected_item.optional_label)
                self.assertEqual(test_item.socket_type, expected_item.socket_type)
                self.assertEqual(test_item.structure_type, expected_item.structure_type)
                self.assertEqual(test_item.is_panel_toggle, expected_item.is_panel_toggle)
                self.assertEqual(test_item.layer_selection_field, expected_item.layer_selection_field)

                # Default value.
                self.assertEqual(test_item.hide_value, expected_item.hide_value)
                self.assertEqual(test_item.hide_in_modifier, expected_item.hide_in_modifier)
                self.assertEqual(test_item.default_input, expected_item.default_input)
                self.assertEqual(test_item.menu_expanded, expected_item.menu_expanded)
                if hasattr(expected_item, "default_value"):
                    self.compare_value(
                        expected_item.bl_socket_idname,
                        test_item.default_value,
                        expected_item.default_value)
                if hasattr(expected_item, "min_value"):
                    self.assertEqual(test_item.min_value, expected_item.min_value)
                if hasattr(expected_item, "max_value"):
                    self.assertEqual(test_item.max_value, expected_item.max_value)
                if hasattr(expected_item, "subtype"):
                    self.assertEqual(test_item.subtype, expected_item.subtype)
                if hasattr(expected_item, "dimensions"):
                    self.assertEqual(test_item.dimensions, expected_item.dimensions)

                # Attribute settings.
                self.assertEqual(test_item.attribute_domain, expected_item.attribute_domain)
                self.assertEqual(test_item.default_attribute_name, expected_item.default_attribute_name)

            if expected_item.item_type == 'PANEL':
                self.assertEqual(test_item.name, expected_item.name)
                self.assertEqual(test_item.description, expected_item.description)
                self.assertEqual(test_item.default_closed, expected_item.default_closed)

    def compare(self, mapping):
        # New sockets may be added to this dictionary while comparing nodes!
        # Make a copy of the original nodes that should be compared.
        orig_test_nodes = list(mapping.node_map.keys())
        orig_expected_nodes = list(mapping.node_map.values())

        for test_tree, expected_tree in mapping.tree_map.items():
            self.compare_tree_interface(test_tree, expected_tree)
        for test_node, expected_node in zip(orig_test_nodes, orig_expected_nodes):
            self.compare_node(test_node, expected_node, mapping)


class NodeMakeGroupTest(AbstractNodeCopyOperatorTest):
    def test_make_group(self):
        test_tree = bpy.data.node_groups["Tests"]
        expected_tree = bpy.data.node_groups["ExpectedMakeGroup"]
        for test_case in filtered_test_cases(test_tree):
            with self.subTest(case=test_case):
                mapping = execute_make_group(test_case, test_tree, expected_tree)
                self.compare(mapping)

    def test_group_insert(self):
        test_tree = bpy.data.node_groups["Tests"]
        expected_tree = bpy.data.node_groups["ExpectedGroupInsert"]
        for test_case in filtered_test_cases(test_tree):
            with self.subTest(case=test_case):
                mapping = execute_group_insert(test_case, test_tree, expected_tree)
                self.compare(mapping)

    def test_ungroup(self):
        # Start with grouped nodes.
        test_tree = bpy.data.node_groups["ExpectedMakeGroup"]
        expected_tree = bpy.data.node_groups["ExpectedUngroup"]
        for test_case in filtered_test_cases(test_tree):
            with self.subTest(case=test_case):
                mapping = execute_ungroup(test_case, test_tree, expected_tree)
                self.compare(mapping)

    def test_group_separate_copy(self):
        # Start with grouped nodes.
        test_tree = bpy.data.node_groups["ExpectedMakeGroup"]
        expected_tree = bpy.data.node_groups["ExpectedGroupSeparateCopy"]
        for test_case in filtered_test_cases(test_tree):
            with self.subTest(case=test_case):
                mapping = execute_group_separate('COPY', test_case, test_tree, expected_tree)
                self.compare(mapping)

    def test_group_separate_move(self):
        # Start with grouped nodes.
        test_tree = bpy.data.node_groups["ExpectedMakeGroup"]
        expected_tree = bpy.data.node_groups["ExpectedGroupSeparateMove"]
        for test_case in filtered_test_cases(test_tree):
            with self.subTest(case=test_case):
                mapping = execute_group_separate('MOVE', test_case, test_tree, expected_tree)
                self.compare(mapping)


################
# Code for generating ground truth test data, sharing functions with test code.

def copy_tree(src_tree, dst_modifier):
    ob = dst_modifier.id_data
    ob.modifiers.active = dst_modifier

    # Clean up old data
    dst_modifier.node_group = None
    # Note: calling bpy.data.orphans_purge() directly does not work for some reason.
    bpy.ops.outliner.orphans_purge()

    dst_tree = src_tree.copy()
    dst_tree.name = dst_modifier.name
    dst_modifier.node_group = dst_tree

    # Ensure a single user action for the tree to avoid destroying animation data.
    if dst_tree.animation_data.action and dst_tree.animation_data.action.users > 1:
        dst_tree.animation_data.action = dst_tree.animation_data.action.copy()

    return dst_tree


def generate_test_data():
    open_test_file()

    test_tree = bpy.data.node_groups["Tests"]
    ob = bpy.data.objects["TestObject"]

    expected_tree__make_group = copy_tree(test_tree, ob.modifiers["ExpectedMakeGroup"])
    # Use result of grouping as starting point for ungrouping and separating.
    for test_case in test_cases(test_tree):
        execute_make_group(test_case, expected_tree__make_group)

    expected_tree__group_insert = copy_tree(test_tree, ob.modifiers["ExpectedGroupInsert"])
    expected_tree__ungroup = copy_tree(expected_tree__make_group, ob.modifiers["ExpectedUngroup"])
    expected_tree__group_separate_copy = copy_tree(expected_tree__make_group, ob.modifiers["ExpectedGroupSeparateCopy"])
    expected_tree__group_separate_move = copy_tree(expected_tree__make_group, ob.modifiers["ExpectedGroupSeparateMove"])
    for test_case in test_cases(test_tree):
        execute_group_insert(test_case, expected_tree__group_insert)
        execute_ungroup(test_case, expected_tree__ungroup)
        execute_group_separate('COPY', test_case, expected_tree__group_separate_copy)
        execute_group_separate('MOVE', test_case, expected_tree__group_separate_move)

    save_test_file()

################


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    parser.add_argument(
        '--generate',
        action='store_true',
        help="Generate ground truth test data instead of running the test")
    parser.add_argument('--subtest', default=None, help="Select a single test case")
    args, remaining = parser.parse_known_args(argv)

    if args.generate:
        generate_test_data()
    else:
        unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
