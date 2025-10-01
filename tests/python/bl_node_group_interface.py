# SPDX-FileCopyrightText: 2021-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys
import unittest
import tempfile

import bpy

args = None


class AbstractNodeGroupInterfaceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir
        cls._tempdir = tempfile.TemporaryDirectory()
        cls.tempdir = pathlib.Path(cls._tempdir.name)

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        'Test dir {0} should exist'.format(self.testdir))

        # Make sure we always start with a known-empty file.
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "empty.blend"))

    def tearDown(self):
        self._tempdir.cleanup()


class NodeGroupInterfaceTests:
    tree_type = None
    group_node_type = None
    # Tree instance where node groups can be added
    main_tree = None

    def make_group(self):
        tree = bpy.data.node_groups.new("test", self.tree_type)
        return tree

    def make_instance(self, tree):
        group_node = self.main_tree.nodes.new(self.group_node_type)
        group_node.node_tree = tree
        return group_node

    def make_group_and_instance(self):
        tree = self.make_group()
        group_node = self.make_instance(tree)
        return tree, group_node

    # Utility method for generating a non-zero default value.
    @staticmethod
    def make_default_socket_value(socket_type):
        if (socket_type == "NodeSocketBool"):
            return True
        elif (socket_type == "NodeSocketColor"):
            return (.5, 1.0, .3, .7)
        elif (socket_type == "NodeSocketFloat"):
            return 1.23
        elif (socket_type == "NodeSocketImage"):
            return bpy.data.images.new("test", 4, 4)
        elif (socket_type == "NodeSocketInt"):
            return -6
        elif (socket_type == "NodeSocketMaterial"):
            return bpy.data.materials.new("test")
        elif (socket_type == "NodeSocketObject"):
            return bpy.data.objects.new("test", bpy.data.meshes.new("test"))
        elif (socket_type == "NodeSocketRotation"):
            return (0.3, 5.0, -42)
        elif (socket_type == "NodeSocketString"):
            return "Hello World!"
        elif (socket_type == "NodeSocketVector"):
            return (4.0, -1.0, 0.0)

    # Utility method returning a comparator for socket values.
    # Not all socket value types are trivially comparable, e.g. colors.
    @staticmethod
    def make_socket_value_comparator(socket_type):
        def cmp_default(test, value, expected):
            test.assertEqual(value, expected, f"Value {value} does not match expected value {expected}")

        def cmp_array(test, value, expected):
            test.assertSequenceEqual(value[:], expected[:], f"Value {value} does not match expected value {expected}")

        if (socket_type in {"NodeSocketBool",
                            "NodeSocketFloat",
                            "NodeSocketImage",
                            "NodeSocketInt",
                            "NodeSocketMaterial",
                            "NodeSocketObject",
                            "NodeSocketRotation",
                            "NodeSocketString"}):
            return cmp_default
        elif (socket_type in {"NodeSocketColor",
                              "NodeSocketVector"}):
            return cmp_array

    def test_empty_nodegroup(self):
        tree, group_node = self.make_group_and_instance()

        self.assertFalse(tree.interface.items_tree, "Interface not empty")
        self.assertFalse(group_node.inputs)
        self.assertFalse(group_node.outputs)

    def do_test_invalid_socket_type(self, socket_type):
        tree = self.make_group()

        with self.assertRaises(TypeError):
            in0 = tree.interface.new_socket("Input 0", socket_type=socket_type, in_out='INPUT')
            self.assertIsNone(in0, f"Socket created for invalid type {socket_type}")
        with self.assertRaises(TypeError):
            out0 = tree.interface.new_socket("Output 0", socket_type=socket_type, in_out='OUTPUT')
            self.assertIsNone(out0, f"Socket created for invalid type {socket_type}")

    def do_test_sockets_in_out(self, socket_type):
        tree, group_node = self.make_group_and_instance()

        out0 = tree.interface.new_socket("Output 0", socket_type=socket_type, in_out='OUTPUT')
        self.assertIsNotNone(out0, f"Could not create socket of type {socket_type}")

        in0 = tree.interface.new_socket("Input 0", socket_type=socket_type, in_out='INPUT')
        self.assertIsNotNone(in0, f"Could not create socket of type {socket_type}")

        in1 = tree.interface.new_socket("Input 1", socket_type=socket_type, in_out='INPUT')
        self.assertIsNotNone(in1, f"Could not create socket of type {socket_type}")

        out1 = tree.interface.new_socket("Output 1", socket_type=socket_type, in_out='OUTPUT')
        self.assertIsNotNone(out1, f"Could not create socket of type {socket_type}")

        self.assertSequenceEqual([(s.name, s.bl_idname) for s in group_node.inputs], [
            ("Input 0", socket_type),
            ("Input 1", socket_type),
        ])
        self.assertSequenceEqual([(s.name, s.bl_idname) for s in group_node.outputs], [
            ("Output 0", socket_type),
            ("Output 1", socket_type),
        ])

    def do_test_user_count(self, value, expected_users):
        if (isinstance(value, bpy.types.ID)):
            self.assertEqual(
                value.users,
                expected_users,
                f"Socket default value has user count {value.users}, expected {expected_users}")

    def do_test_socket_type(self, socket_type):
        default_value = self.make_default_socket_value(socket_type)
        compare_value = self.make_socket_value_comparator(socket_type)

        # Create the tree first, add sockets, then create a group instance.
        # That way the new instance should reflect the expected default values.
        tree = self.make_group()

        in0 = tree.interface.new_socket("Input 0", socket_type=socket_type, in_out='INPUT')
        if default_value is not None:
            in0.default_value = default_value
        out0 = tree.interface.new_socket("Output 0", socket_type=socket_type, in_out='OUTPUT')
        self.assertIsNotNone(in0, f"Could not create socket of type {socket_type}")
        self.assertIsNotNone(out0, f"Could not create socket of type {socket_type}")

        # Now make a node group instance to check default values.
        group_node = self.make_instance(tree)
        if compare_value:
            compare_value(self, group_node.inputs[0].default_value, in0.default_value)

        # Test ID user count after assigning.
        if (hasattr(in0, "default_value")):
            # The default value is stored in both the interface and node, it should have 2 users now.
            self.do_test_user_count(in0.default_value, 2)

        # Copy sockets
        in1 = tree.interface.copy(in0)
        out1 = tree.interface.copy(out0)
        self.assertIsNotNone(in1, "Could not copy socket")
        self.assertIsNotNone(out1, "Could not copy socket")
        # User count on default values should increment by 2 after copy,
        # one user for the interface and one for the group node instance.
        if (hasattr(in1, "default_value")):
            self.do_test_user_count(in1.default_value, 4)

    # Classic outputs..inputs socket layout
    def do_test_items_order_classic(self, socket_type):
        tree, group_node = self.make_group_and_instance()

        tree.interface.new_socket("Output 0", socket_type=socket_type, in_out='OUTPUT')
        tree.interface.new_socket("Input 0", socket_type=socket_type, in_out='INPUT')

        self.assertSequenceEqual([(s.name, s.item_type) for s in tree.interface.items_tree], [
            ("Output 0", 'SOCKET'),
            ("Input 0", 'SOCKET'),
        ])
        self.assertSequenceEqual([s.name for s in group_node.inputs], [
            "Input 0",
        ])
        self.assertSequenceEqual([s.name for s in group_node.outputs], [
            "Output 0",
        ])
        # XXX currently no panel state access on node instances.
        # self.assertFalse(group_node.panels)

    # Mixed sockets and panels
    def do_test_items_order_mixed_with_panels(self, socket_type):
        tree, group_node = self.make_group_and_instance()

        tree.interface.new_panel("Panel 0")
        tree.interface.new_socket("Input 0", socket_type=socket_type, in_out='INPUT')
        tree.interface.new_socket("Output 0", socket_type=socket_type, in_out='OUTPUT')
        tree.interface.new_panel("Panel 1")
        tree.interface.new_socket("Input 1", socket_type=socket_type, in_out='INPUT')
        tree.interface.new_panel("Panel 2")
        tree.interface.new_socket("Output 1", socket_type=socket_type, in_out='OUTPUT')
        tree.interface.new_panel("Panel 3")

        # Panels after sockets
        self.assertSequenceEqual([(s.name, s.item_type) for s in tree.interface.items_tree], [
            ("Output 0", 'SOCKET'),
            ("Output 1", 'SOCKET'),
            ("Input 0", 'SOCKET'),
            ("Input 1", 'SOCKET'),
            ("Panel 0", 'PANEL'),
            ("Panel 1", 'PANEL'),
            ("Panel 2", 'PANEL'),
            ("Panel 3", 'PANEL'),
        ])
        self.assertSequenceEqual([s.name for s in group_node.inputs], [
            "Input 0",
            "Input 1",
        ])
        self.assertSequenceEqual([s.name for s in group_node.outputs], [
            "Output 0",
            "Output 1",
        ])
        # XXX currently no panel state access on node instances.
        # self.assertSequenceEqual([p.name for p in group_node.panels], [
        #     "Panel 0",
        #     "Panel 1",
        #     "Panel 2",
        #     "Panel 3",
        #     ])

    def do_test_add(self, socket_type):
        tree, group_node = self.make_group_and_instance()

        in0 = tree.interface.new_socket("Input 0", socket_type=socket_type, in_out='INPUT')
        self.assertSequenceEqual(tree.interface.items_tree, [in0])
        self.assertSequenceEqual([s.name for s in group_node.inputs], ["Input 0"])
        self.assertSequenceEqual([s.name for s in group_node.outputs], [])

        out0 = tree.interface.new_socket("Output 0", socket_type=socket_type, in_out='OUTPUT')
        self.assertSequenceEqual(tree.interface.items_tree, [out0, in0])
        self.assertSequenceEqual([s.name for s in group_node.inputs], ["Input 0"])
        self.assertSequenceEqual([s.name for s in group_node.outputs], ["Output 0"])

        panel0 = tree.interface.new_panel("Panel 0")
        self.assertSequenceEqual(tree.interface.items_tree, [out0, in0, panel0])
        self.assertSequenceEqual([s.name for s in group_node.inputs], ["Input 0"])
        self.assertSequenceEqual([s.name for s in group_node.outputs], ["Output 0"])

        # Add items to the panel.
        in1 = tree.interface.new_socket("Input 1", socket_type=socket_type, in_out='INPUT', parent=panel0)
        self.assertSequenceEqual(tree.interface.items_tree, [out0, in0, panel0, in1])
        self.assertSequenceEqual([s.name for s in group_node.inputs], ["Input 0", "Input 1"])
        self.assertSequenceEqual([s.name for s in group_node.outputs], ["Output 0"])

        out1 = tree.interface.new_socket("Output 1", socket_type=socket_type, in_out='OUTPUT', parent=panel0)
        self.assertSequenceEqual(tree.interface.items_tree, [out0, in0, panel0, out1, in1])
        self.assertSequenceEqual([s.name for s in group_node.inputs], ["Input 0", "Input 1"])
        self.assertSequenceEqual([s.name for s in group_node.outputs], ["Output 0", "Output 1"])

    def do_test_remove(self, socket_type):
        tree, group_node = self.make_group_and_instance()

        in0 = tree.interface.new_socket("Input 0", socket_type=socket_type, in_out='INPUT')
        out0 = tree.interface.new_socket("Output 0", socket_type=socket_type, in_out='OUTPUT')
        panel0 = tree.interface.new_panel("Panel 0")
        in1 = tree.interface.new_socket("Input 1", socket_type=socket_type, in_out='INPUT', parent=panel0)
        out1 = tree.interface.new_socket("Output 1", socket_type=socket_type, in_out='OUTPUT', parent=panel0)
        panel1 = tree.interface.new_panel("Panel 1")
        in2 = tree.interface.new_socket("Input 2", socket_type=socket_type, in_out='INPUT', parent=panel1)
        out2 = tree.interface.new_socket("Output 2", socket_type=socket_type, in_out='OUTPUT', parent=panel1)
        panel2 = tree.interface.new_panel("Panel 2")

        self.assertSequenceEqual(tree.interface.items_tree, [out0, in0, panel0, out1, in1, panel1, out2, in2, panel2])
        self.assertSequenceEqual([s.name for s in group_node.inputs], ["Input 0", "Input 1", "Input 2"])
        self.assertSequenceEqual([s.name for s in group_node.outputs], ["Output 0", "Output 1", "Output 2"])

        # Remove from root panel.
        tree.interface.remove(in0)
        self.assertSequenceEqual(tree.interface.items_tree, [out0, panel0, out1, in1, panel1, out2, in2, panel2])
        self.assertSequenceEqual([s.name for s in group_node.inputs], ["Input 1", "Input 2"])
        self.assertSequenceEqual([s.name for s in group_node.outputs], ["Output 0", "Output 1", "Output 2"])

        # Removing a panel should move content to the parent.
        tree.interface.remove(panel0)
        self.assertSequenceEqual(tree.interface.items_tree, [out0, out1, in1, panel1, out2, in2, panel2])
        self.assertSequenceEqual([s.name for s in group_node.inputs], ["Input 1", "Input 2"])
        self.assertSequenceEqual([s.name for s in group_node.outputs], ["Output 0", "Output 1", "Output 2"])

        tree.interface.remove(out0)
        self.assertSequenceEqual(tree.interface.items_tree, [out1, in1, panel1, out2, in2, panel2])
        self.assertSequenceEqual([s.name for s in group_node.inputs], ["Input 1", "Input 2"])
        self.assertSequenceEqual([s.name for s in group_node.outputs], ["Output 1", "Output 2"])

        # Remove content from panel
        tree.interface.remove(out2)
        self.assertSequenceEqual(tree.interface.items_tree, [out1, in1, panel1, in2, panel2])
        self.assertSequenceEqual([s.name for s in group_node.inputs], ["Input 1", "Input 2"])
        self.assertSequenceEqual([s.name for s in group_node.outputs], ["Output 1"])

        # Remove a panel and its content
        tree.interface.remove(panel1, move_content_to_parent=False)
        self.assertSequenceEqual(tree.interface.items_tree, [out1, in1, panel2])
        self.assertSequenceEqual([s.name for s in group_node.inputs], ["Input 1"])
        self.assertSequenceEqual([s.name for s in group_node.outputs], ["Output 1"])

        # Remove empty panel
        tree.interface.remove(panel2)
        self.assertSequenceEqual(tree.interface.items_tree, [out1, in1])
        self.assertSequenceEqual([s.name for s in group_node.inputs], ["Input 1"])
        self.assertSequenceEqual([s.name for s in group_node.outputs], ["Output 1"])

    def do_test_move(self, socket_type):
        tree, group_node = self.make_group_and_instance()

        in0 = tree.interface.new_socket("Input 0", socket_type=socket_type, in_out='INPUT')
        in1 = tree.interface.new_socket("Input 1", socket_type=socket_type, in_out='INPUT', parent=panel0)
        out0 = tree.interface.new_socket("Output 0", socket_type=socket_type, in_out='OUTPUT')
        out1 = tree.interface.new_socket("Output 1", socket_type=socket_type, in_out='OUTPUT', parent=panel0)
        panel0 = tree.interface.new_panel("Panel 0")
        panel1 = tree.interface.new_panel("Panel 1")


class GeometryNodeGroupInterfaceTest(AbstractNodeGroupInterfaceTest, NodeGroupInterfaceTests):
    tree_type = "GeometryNodeTree"
    group_node_type = "GeometryNodeGroup"

    def setUp(self):
        super().setUp()
        self.main_tree = bpy.data.node_groups.new("main", self.tree_type)

    def test_sockets_in_out(self):
        self.do_test_sockets_in_out("NodeSocketFloat")

    def test_all_socket_types(self):
        self.do_test_invalid_socket_type("INVALID_SOCKET_TYPE_11!1")
        self.do_test_socket_type("NodeSocketBool")
        self.do_test_socket_type("NodeSocketCollection")
        self.do_test_socket_type("NodeSocketColor")
        self.do_test_socket_type("NodeSocketFloat")
        self.do_test_socket_type("NodeSocketGeometry")
        self.do_test_socket_type("NodeSocketImage")
        self.do_test_socket_type("NodeSocketInt")
        self.do_test_socket_type("NodeSocketMaterial")
        self.do_test_socket_type("NodeSocketObject")
        self.do_test_socket_type("NodeSocketRotation")
        self.do_test_invalid_socket_type("NodeSocketShader")
        self.do_test_socket_type("NodeSocketString")
        self.do_test_invalid_socket_type("NodeSocketTexture")
        self.do_test_socket_type("NodeSocketVector")
        self.do_test_invalid_socket_type("NodeSocketVirtual")

    def test_items_order_classic(self):
        self.do_test_items_order_classic("NodeSocketFloat")

    def test_items_order_mixed_with_panels(self):
        self.do_test_items_order_mixed_with_panels("NodeSocketFloat")

    def test_add(self):
        self.do_test_add("NodeSocketFloat")

    def test_remove(self):
        self.do_test_remove("NodeSocketFloat")


class ShaderNodeGroupInterfaceTest(AbstractNodeGroupInterfaceTest, NodeGroupInterfaceTests):
    tree_type = "ShaderNodeTree"
    group_node_type = "ShaderNodeGroup"

    def setUp(self):
        super().setUp()
        self.material = bpy.data.materials.new("test")
        self.main_tree = self.material.node_tree

    def test_invalid_socket_type(self):
        self.do_test_invalid_socket_type("INVALID_SOCKET_TYPE_11!1")

    def test_sockets_in_out(self):
        self.do_test_sockets_in_out("NodeSocketFloat")

    def test_all_socket_types(self):
        self.do_test_socket_type("NodeSocketBool")
        self.do_test_invalid_socket_type("NodeSocketCollection")
        self.do_test_socket_type("NodeSocketColor")
        self.do_test_socket_type("NodeSocketFloat")
        self.do_test_invalid_socket_type("NodeSocketGeometry")
        self.do_test_invalid_socket_type("NodeSocketImage")
        self.do_test_socket_type("NodeSocketInt")
        self.do_test_invalid_socket_type("NodeSocketMaterial")
        self.do_test_invalid_socket_type("NodeSocketObject")
        self.do_test_invalid_socket_type("NodeSocketRotation")
        self.do_test_socket_type("NodeSocketShader")
        self.do_test_invalid_socket_type("NodeSocketString")
        self.do_test_invalid_socket_type("NodeSocketTexture")
        self.do_test_socket_type("NodeSocketVector")
        self.do_test_invalid_socket_type("NodeSocketVirtual")

    def test_items_order_classic(self):
        self.do_test_items_order_classic("NodeSocketFloat")

    def test_items_order_mixed_with_panels(self):
        self.do_test_items_order_mixed_with_panels("NodeSocketFloat")

    def test_add(self):
        self.do_test_add("NodeSocketFloat")

    def test_remove(self):
        self.do_test_remove("NodeSocketFloat")


class CompositorNodeGroupInterfaceTest(AbstractNodeGroupInterfaceTest, NodeGroupInterfaceTests):
    tree_type = "CompositorNodeTree"
    group_node_type = "CompositorNodeGroup"

    def setUp(self):
        super().setUp()
        self.scene = bpy.data.scenes.new("test")
        self.main_tree = bpy.data.node_groups.new("test node tree", "CompositorNodeTree")
        self.scene.compositing_node_group = self.main_tree

    def test_invalid_socket_type(self):
        self.do_test_invalid_socket_type("INVALID_SOCKET_TYPE_11!1")

    def test_sockets_in_out(self):
        self.do_test_sockets_in_out("NodeSocketFloat")

    def test_all_socket_types(self):
        self.do_test_socket_type("NodeSocketBool")
        self.do_test_invalid_socket_type("NodeSocketCollection")
        self.do_test_socket_type("NodeSocketColor")
        self.do_test_socket_type("NodeSocketFloat")
        self.do_test_invalid_socket_type("NodeSocketGeometry")
        self.do_test_invalid_socket_type("NodeSocketImage")
        self.do_test_socket_type("NodeSocketInt")
        self.do_test_invalid_socket_type("NodeSocketMaterial")
        self.do_test_invalid_socket_type("NodeSocketObject")
        self.do_test_invalid_socket_type("NodeSocketRotation")
        self.do_test_invalid_socket_type("NodeSocketShader")
        self.do_test_socket_type("NodeSocketString")
        self.do_test_invalid_socket_type("NodeSocketTexture")
        self.do_test_socket_type("NodeSocketVector")
        self.do_test_invalid_socket_type("NodeSocketVirtual")

    def test_items_order_classic(self):
        self.do_test_items_order_classic("NodeSocketFloat")

    def test_items_order_mixed_with_panels(self):
        self.do_test_items_order_mixed_with_panels("NodeSocketFloat")

    def test_add(self):
        self.do_test_add("NodeSocketFloat")

    def test_remove(self):
        self.do_test_remove("NodeSocketFloat")


class NodeTreeItemsIteratorTest(AbstractNodeGroupInterfaceTest, NodeGroupInterfaceTests):
    tree_type = "ShaderNodeTree"
    group_node_type = "ShaderNodeGroup"

    def setUp(self):
        super().setUp()
        self.material = bpy.data.materials.new("test")
        self.main_tree = self.material.node_tree

    # Regression test for changes while iterating over tree interface items (#143551).
    # The iterator should remain valid when changing properties of a tree item.
    def test_items_iterator(self):
        tree, group_node = self.make_group_and_instance()

        tree.interface.new_socket("Input 0", socket_type="NodeSocketFloat", in_out='INPUT')
        tree.interface.new_socket("Input 1", socket_type="NodeSocketBool", in_out='INPUT')
        # The cache vector has a fixed buffer for small sizes, add enough sockets to force reallocation.
        for i in range(20):
            tree.interface.new_socket(f"Input {2+i}", socket_type="NodeSocketColor", in_out='INPUT')

        # Iterate over items and change properties. The loop iterator must remain valid.
        for item in tree.interface.items_tree:
            if item.socket_type == "NodeSocketFloat":
                item.default_value = 500.0
            elif item.socket_type == "NodeSocketColor":
                item.default_value = (1, 0, 0, 1)
            elif item.socket_type == "NodeSocketBool":
                item.default_value = True


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
