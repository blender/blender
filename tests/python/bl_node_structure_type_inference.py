# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys
import tempfile
import bpy
import unittest

args = None


class StructureTypeInferenceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        "Test dir {0} should exist".format(self.testdir))

    def load_testfile(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "structure_type_inference.blend"))

    def assertDynamic(self, socket):
        self.assertEqual(socket.inferred_structure_type, "DYNAMIC")

    def assertSingle(self, socket):
        self.assertEqual(socket.inferred_structure_type, "SINGLE")

    def assertField(self, socket):
        self.assertEqual(socket.inferred_structure_type, "FIELD")

    def assertGrid(self, socket):
        self.assertEqual(socket.inferred_structure_type, "GRID")

    def test_empty_group(self):
        self.load_testfile()
        tree = bpy.data.node_groups["test_empty_group"]

        node = tree.nodes["Group Input"]
        self.assertSingle(node.outputs["Geometry"])
        self.assertDynamic(node.outputs["Value"])

        node = tree.nodes["Group Output"]
        self.assertSingle(node.inputs["Geometry"])
        self.assertSingle(node.inputs["Value"])

    def test_math_node(self):
        self.load_testfile()
        tree = bpy.data.node_groups["test_math_node"]

        node = tree.nodes["Group Input"]
        self.assertDynamic(node.outputs["A"])
        self.assertDynamic(node.outputs["B"])

        node = tree.nodes["Group Output"]
        self.assertDynamic(node.inputs["Out"])

    def test_cube_node(self):
        self.load_testfile()
        tree = bpy.data.node_groups["test_cube_node"]

        node = tree.nodes["Group Input"]
        self.assertSingle(node.outputs["Size"])
        self.assertSingle(node.outputs["Vertices X"])
        self.assertSingle(node.outputs["Vertices Y"])
        self.assertSingle(node.outputs["Vertices Z"])

        node = tree.nodes["Group Output"]
        self.assertSingle(node.inputs["Mesh"])
        self.assertField(node.inputs["UV Map"])

    def test_set_position_node(self):
        self.load_testfile()
        tree = bpy.data.node_groups["test_set_position_node"]

        node = tree.nodes["Group Input"]
        self.assertSingle(node.outputs["Geometry"])
        self.assertField(node.outputs["Selection"])
        self.assertField(node.outputs["Position"])
        self.assertField(node.outputs["Offset"])

        node = tree.nodes["Group Output"]
        self.assertSingle(node.inputs["Geometry"])

    def test_cube_with_math_node(self):
        self.load_testfile()
        tree = bpy.data.node_groups["test_cube_with_math_node"]

        node = tree.nodes["Group Input"]
        self.assertSingle(node.outputs["A"])
        self.assertSingle(node.outputs["B"])

        node = tree.nodes["Group Output"]
        self.assertSingle(node.inputs["Mesh"])
        self.assertField(node.inputs["UV Map"])

    def test_output_field(self):
        self.load_testfile()
        tree = bpy.data.node_groups["test_output_field"]

        node = tree.nodes["Group Output"]
        self.assertField(node.inputs["Position"])
        self.assertField(node.inputs["Normal 1"])
        self.assertField(node.inputs["Normal 2"])

    def test_add_all_types(self):
        self.load_testfile()
        tree = bpy.data.node_groups["test_add_all_types"]

        node = tree.nodes["Group Input"]
        self.assertDynamic(node.outputs["Auto"])
        self.assertSingle(node.outputs["Single"])
        self.assertDynamic(node.outputs["Dynamic"])
        self.assertField(node.outputs["Field"])
        self.assertGrid(node.outputs["Grid"])

        node = tree.nodes["Group Output"]

        self.assertDynamic(node.inputs["auto+auto"])
        self.assertDynamic(node.inputs["auto+single"])
        self.assertDynamic(node.inputs["auto+dynamic"])
        self.assertDynamic(node.inputs["auto+field"])
        self.assertGrid(node.inputs["auto+grid"])

        self.assertSingle(node.inputs["single+single"])
        self.assertDynamic(node.inputs["single+dynamic"])
        self.assertField(node.inputs["single+field"])
        self.assertGrid(node.inputs["single+grid"])

        self.assertDynamic(node.inputs["dynamic+dynamic"])
        self.assertDynamic(node.inputs["dynamic+field"])
        self.assertGrid(node.inputs["dynamic+grid"])

        self.assertField(node.inputs["field+field"])
        self.assertGrid(node.inputs["field+grid"])

        self.assertGrid(node.inputs["grid+grid"])

    def test_requirement_combinations(self):
        self.load_testfile()
        tree = bpy.data.node_groups["test_requirement_combinations"]

        node = tree.nodes["Group Input"]

        self.assertDynamic(node.outputs["none"])
        self.assertDynamic(node.outputs["dynamic"])
        self.assertSingle(node.outputs["single"])
        self.assertField(node.outputs["field"])
        self.assertGrid(node.outputs["grid"])

        self.assertDynamic(node.outputs["none+dynamic"])
        self.assertSingle(node.outputs["none+single"])
        self.assertField(node.outputs["none+field"])
        self.assertGrid(node.outputs["none+grid"])

        self.assertDynamic(node.outputs["dynamic+dynamic"])
        self.assertSingle(node.outputs["dynamic+single"])
        self.assertField(node.outputs["dynamic+field"])
        self.assertGrid(node.outputs["dynamic+grid"])

        self.assertSingle(node.outputs["single+single"])
        self.assertSingle(node.outputs["single+dynamic"])
        self.assertSingle(node.outputs["single+field"])
        self.assertDynamic(node.outputs["single+grid"])

        self.assertField(node.outputs["field+field"])
        self.assertSingle(node.outputs["field+single"])
        self.assertField(node.outputs["field+dynamic"])
        self.assertDynamic(node.outputs["field+grid"])

        self.assertGrid(node.outputs["grid+grid"])
        self.assertDynamic(node.outputs["grid+single"])
        self.assertDynamic(node.outputs["grid+field"])
        self.assertGrid(node.outputs["grid+dynamic"])

        self.assertDynamic(node.outputs["dynamic+single+field"])
        self.assertDynamic(node.outputs["single+field+grid"])
        self.assertDynamic(node.outputs["dynamic+field+grid"])
        self.assertDynamic(node.outputs["dynamic+single+grid"])

        self.assertDynamic(node.outputs["dynamic+single+field+grid"])

    def test_simulation_zone(self):
        self.load_testfile()
        tree = bpy.data.node_groups["test_simulation_zone"]

        node = tree.nodes["Group Input"]
        self.assertSingle(node.outputs["single 1"])
        self.assertSingle(node.outputs["single 2"])
        self.assertSingle(node.outputs["single 3"])
        self.assertSingle(node.outputs["single 4"])
        self.assertDynamic(node.outputs["dynamic"])

    def test_repeat_zone(self):
        self.load_testfile()
        tree = bpy.data.node_groups["test_repeat_zone"]

        node = tree.nodes["Group Input"]
        self.assertSingle(node.outputs["Iterations"])
        self.assertSingle(node.outputs["single 1"])
        self.assertSingle(node.outputs["single 2"])
        self.assertSingle(node.outputs["single 3"])
        self.assertSingle(node.outputs["single 4"])

        node = tree.nodes["Group Output"]
        self.assertSingle(node.inputs["single 1"])
        self.assertSingle(node.inputs["single 2"])
        self.assertSingle(node.inputs["single 3"])
        self.assertSingle(node.inputs["single 4"])
        self.assertField(node.inputs["field 1"])
        self.assertField(node.inputs["field 2"])
        self.assertField(node.inputs["field 3"])
        self.assertGrid(node.inputs["grid 1"])
        self.assertGrid(node.inputs["grid 2"])
        self.assertGrid(node.inputs["grid 3"])

    def test_closure_zone(self):
        self.load_testfile()
        tree = bpy.data.node_groups["test_closure_zone"]

        node = tree.nodes["Closure Input"]
        self.assertField(node.outputs["field"])
        self.assertSingle(node.outputs["single"])

        node = tree.nodes["Closure Output"]
        self.assertGrid(node.inputs["grid"])
        self.assertField(node.inputs["field"])


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
