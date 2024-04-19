# SPDX-FileCopyrightText: 2021-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys
import unittest
import tempfile

import bpy

args = None


class FieldTypeInferenceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.testdir = args.testdir
        cls._tempdir = tempfile.TemporaryDirectory()
        cls.tempdir = pathlib.Path(cls._tempdir.name)

    def setUp(self):
        self.assertTrue(self.testdir.exists(),
                        'Test dir {0} should exist'.format(self.testdir))

    def tearDown(self):
        self._tempdir.cleanup()

    # Note: field types are not exposed in RNA yet, so we have to use the
    # socket display shape as a proxy for testing.

    @staticmethod
    def field_type_name(socket):
        if socket.display_shape == 'CIRCLE':
            return 'Single Value'
        elif socket.display_shape == 'DIAMOND':
            return 'Field'
        elif socket.display_shape == 'DIAMOND_DOT':
            return 'Field or Single Value'
        else:
            return 'Unknown'

    def assert_value_socket(self, socket):
        self.assertEqual(
            socket.display_shape,
            'CIRCLE',
            "Socket {} must be Single Value, but is {}".format(
                socket.name,
                self.field_type_name(socket)))

    def assert_field_socket(self, socket):
        self.assertEqual(
            socket.display_shape,
            'DIAMOND',
            "Socket {} must be Field, but is {}".format(
                socket.name,
                self.field_type_name(socket)))

    def assert_value_or_field_socket(self, socket):
        self.assertEqual(
            socket.display_shape,
            'DIAMOND_DOT',
            "Socket {} must be Field or Single Value, but is {}".format(
                socket.name,
                self.field_type_name(socket)))

    def find_input_link(self, socket):
        for link in socket.id_data.links:
            if link.to_socket == socket:
                return link

    def assert_input_link_valid(self, socket):
        link = self.find_input_link(socket)
        self.assertIsNotNone(link, "Expected input link to socket {}".format(socket.name))
        self.assertTrue(link.is_valid)

    def assert_input_link_invalid(self, socket):
        link = self.find_input_link(socket)
        self.assertIsNotNone(link, "Expected input link to socket {}".format(socket.name))
        # XXX links currently are not actually flagged as invalid by field type inferencing,
        # but rather just draw links in red based on socket symbols.
        # This test should work once links are actually made invalid by type inferencing.
        # self.assertFalse(link.is_valid)
        is_red_link = link.from_socket.display_shape == 'DIAMOND' and link.to_socket.display_shape == 'CIRCLE'
        self.assertTrue(is_red_link)

    def load_testfile(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "field_type_inference.blend"))
        self.assertEqual(bpy.data.version, (4, 1, 0))

    def test_unconnected_nodes(self):
        self.load_testfile()
        tree = bpy.data.node_groups['Geometry Nodes']

        node = tree.nodes['Group Input Unconnected']
        self.assertEqual(node.bl_idname, "NodeGroupInput")
        self.assert_value_socket(node.outputs['Geometry'])
        # Field-capable group inputs are resolved as fields for maximum compatibility.
        self.assert_field_socket(node.outputs['Socket'])

        node = tree.nodes['Group Output Unconnected']
        self.assertEqual(node.bl_idname, "NodeGroupOutput")
        self.assert_value_socket(node.inputs['Geometry'])
        self.assert_value_or_field_socket(node.inputs['Socket'])

        node = tree.nodes['Mix Unconnected']
        self.assertEqual(node.bl_idname, "ShaderNodeMix")
        self.assert_value_or_field_socket(node.inputs['Factor'])
        self.assert_value_or_field_socket(node.inputs['A'])
        self.assert_value_or_field_socket(node.inputs['B'])
        self.assert_value_or_field_socket(node.outputs['Result'])

        node = tree.nodes['Cube Unconnected']
        self.assertEqual(node.bl_idname, "GeometryNodeMeshCube")
        self.assert_value_socket(node.inputs['Size'])
        self.assert_value_socket(node.inputs['Vertices X'])
        self.assert_value_socket(node.inputs['Vertices Y'])
        self.assert_value_socket(node.inputs['Vertices Z'])
        self.assert_value_socket(node.outputs['Mesh'])
        self.assert_field_socket(node.outputs['UV Map'])

        node = tree.nodes['Capture Attribute Unconnected']
        self.assertEqual(node.bl_idname, "GeometryNodeCaptureAttribute")
        self.assert_value_socket(node.inputs['Geometry'])
        self.assert_value_or_field_socket(node.inputs['Value'])
        self.assert_value_socket(node.outputs['Geometry'])
        # Capture Attribute output is always a field.
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Set Position Unconnected']
        self.assertEqual(node.bl_idname, "GeometryNodeSetPosition")
        self.assert_value_socket(node.inputs['Geometry'])
        self.assert_value_or_field_socket(node.inputs['Selection'])
        self.assert_field_socket(node.inputs['Position'])
        self.assert_value_or_field_socket(node.inputs['Offset'])
        self.assert_value_socket(node.outputs['Geometry'])

    def test_simple_connections(self):
        self.load_testfile()
        tree = bpy.data.node_groups['Geometry Nodes']

        node = tree.nodes['Group Input Simple']
        self.assertEqual(node.bl_idname, "NodeGroupInput")
        self.assert_value_socket(node.outputs['Geometry'])
        self.assert_field_socket(node.outputs['Socket'])

        node = tree.nodes['Group Output Simple']
        self.assertEqual(node.bl_idname, "NodeGroupOutput")
        self.assert_value_socket(node.inputs['Geometry'])
        self.assert_input_link_valid(node.inputs['Geometry'])
        # Resolved as field socket.
        self.assert_field_socket(node.inputs['Socket'])
        self.assert_input_link_valid(node.inputs['Socket'])

        node = tree.nodes['Capture Attribute Simple']
        self.assertEqual(node.bl_idname, "GeometryNodeCaptureAttribute")
        self.assert_value_socket(node.inputs['Geometry'])
        # Resolved as field socket.
        self.assert_field_socket(node.inputs['Value'])
        self.assert_input_link_valid(node.inputs['Value'])
        self.assert_value_socket(node.outputs['Geometry'])
        self.assert_field_socket(node.outputs['Attribute'])

    def test_single_value_to_field(self):
        self.load_testfile()
        tree = bpy.data.node_groups['Geometry Nodes']

        node = tree.nodes['Domain Size Value']
        self.assertEqual(node.bl_idname, "GeometryNodeAttributeDomainSize")
        self.assert_value_socket(node.inputs['Geometry'])
        self.assert_value_socket(node.outputs['Point Count'])
        self.assert_value_socket(node.outputs['Edge Count'])
        self.assert_value_socket(node.outputs['Face Count'])
        self.assert_value_socket(node.outputs['Face Corner Count'])

        node = tree.nodes['Capture Attribute Value']
        self.assertEqual(node.bl_idname, "GeometryNodeCaptureAttribute")
        self.assert_value_socket(node.inputs['Geometry'])
        self.assert_value_or_field_socket(node.inputs['Value'])
        self.assert_input_link_valid(node.inputs['Value'])
        self.assert_value_socket(node.outputs['Geometry'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Set Position Value']
        self.assertEqual(node.bl_idname, "GeometryNodeSetPosition")
        self.assert_value_socket(node.inputs['Geometry'])
        self.assert_value_or_field_socket(node.inputs['Selection'])
        self.assert_value_or_field_socket(node.inputs['Position'])
        self.assert_input_link_valid(node.inputs['Position'])
        self.assert_value_or_field_socket(node.inputs['Offset'])
        self.assert_value_socket(node.outputs['Geometry'])

    def test_field_to_single_value(self):
        self.load_testfile()
        tree = bpy.data.node_groups['Geometry Nodes']

        node = tree.nodes['Capture Attribute Field']
        self.assertEqual(node.bl_idname, "GeometryNodeCaptureAttribute")
        self.assert_value_socket(node.inputs['Geometry'])
        self.assert_value_or_field_socket(node.inputs['Value'])
        self.assert_value_socket(node.outputs['Geometry'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Cube Field']
        self.assertEqual(node.bl_idname, "GeometryNodeMeshCube")
        self.assert_value_socket(node.inputs['Size'])
        self.assert_value_socket(node.inputs['Vertices X'])
        self.assert_input_link_invalid(node.inputs['Vertices X'])
        self.assert_value_socket(node.inputs['Vertices Y'])
        self.assert_value_socket(node.inputs['Vertices Z'])
        self.assert_value_socket(node.outputs['Mesh'])
        self.assert_field_socket(node.outputs['UV Map'])

    def test_zones(self):
        self.load_testfile()
        tree = bpy.data.node_groups['Geometry Nodes']

        node = tree.nodes['Simulation Input Zone1']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationInput")
        self.assert_value_or_field_socket(node.inputs['Attribute'])
        self.assert_value_or_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Output Zone1']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationOutput")
        self.assert_value_or_field_socket(node.inputs['Attribute'])
        self.assert_value_or_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Input Zone2']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationInput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_input_link_valid(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Output Zone2']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationOutput")
        # Input can still be a single value.
        self.assert_value_or_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Input Zone3']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationInput")
        # Input can still be a single value.
        self.assert_value_or_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Output Zone3']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationOutput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_input_link_valid(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Input Zone4']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationInput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_input_link_valid(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Output Zone4']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationOutput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_input_link_valid(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

    def test_zone_errors(self):
        self.load_testfile()
        tree = bpy.data.node_groups['Geometry Nodes']

        node = tree.nodes['Simulation Input ZoneError1']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationInput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Output ZoneError1']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationOutput")
        self.assert_value_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Input ZoneError2']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationInput")
        self.assert_value_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Output ZoneError2']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationOutput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Input ZoneError3']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationInput")
        self.assert_value_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Output ZoneError3']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationOutput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Input ZoneError4']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationInput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Output ZoneError4']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationOutput")
        self.assert_value_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

    def test_nested_nodes(self):
        self.load_testfile()
        tree = bpy.data.node_groups['Geometry Nodes']

        # Nested Zones 1.

        node = tree.nodes['Repeat Input NestedZone1']
        self.assertEqual(node.bl_idname, "GeometryNodeRepeatInput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Repeat Output NestedZone1']
        self.assertEqual(node.bl_idname, "GeometryNodeRepeatOutput")
        self.assert_value_or_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Input NestedZone1']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationInput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Output NestedZone1']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationOutput")
        self.assert_value_or_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Repeat Input2 NestedZone1']
        self.assertEqual(node.bl_idname, "GeometryNodeRepeatInput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Repeat Output2 NestedZone1']
        self.assertEqual(node.bl_idname, "GeometryNodeRepeatOutput")
        self.assert_value_or_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        # Nested Zones 2.

        node = tree.nodes['Repeat Input NestedZone2']
        self.assertEqual(node.bl_idname, "GeometryNodeRepeatInput")
        self.assert_value_or_field_socket(node.inputs['Attribute'])
        self.assert_value_or_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Repeat Output NestedZone2']
        self.assertEqual(node.bl_idname, "GeometryNodeRepeatOutput")
        self.assert_value_or_field_socket(node.inputs['Attribute'])
        self.assert_value_or_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Input NestedZone2']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationInput")
        self.assert_value_or_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Simulation Output NestedZone2']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationOutput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Repeat Input2 NestedZone2']
        self.assertEqual(node.bl_idname, "GeometryNodeRepeatInput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

        node = tree.nodes['Repeat Output2 NestedZone2']
        self.assertEqual(node.bl_idname, "GeometryNodeRepeatOutput")
        self.assert_value_or_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute'])

    def test_zones_multiple_items(self):
        self.load_testfile()
        tree = bpy.data.node_groups['Geometry Nodes']

        node = tree.nodes['Simulation Input ZoneIterate1']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationInput")
        self.assert_field_socket(node.inputs['Attribute'])
        self.assert_value_or_field_socket(node.inputs['Attribute.001'])
        self.assert_value_or_field_socket(node.inputs['Attribute.002'])
        self.assert_value_or_field_socket(node.inputs['Attribute.003'])
        self.assert_value_or_field_socket(node.inputs['Attribute.004'])
        self.assert_field_socket(node.outputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute.001'])
        self.assert_field_socket(node.outputs['Attribute.002'])
        self.assert_field_socket(node.outputs['Attribute.003'])
        self.assert_field_socket(node.outputs['Attribute.004'])

        node = tree.nodes['Simulation Output ZoneIterate1']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationOutput")
        self.assert_value_or_field_socket(node.inputs['Attribute'])
        self.assert_field_socket(node.inputs['Attribute.001'])
        self.assert_field_socket(node.inputs['Attribute.002'])
        self.assert_field_socket(node.inputs['Attribute.003'])
        self.assert_field_socket(node.inputs['Attribute.004'])
        self.assert_field_socket(node.outputs['Attribute'])
        self.assert_field_socket(node.outputs['Attribute.001'])
        self.assert_field_socket(node.outputs['Attribute.002'])
        self.assert_field_socket(node.outputs['Attribute.003'])
        self.assert_field_socket(node.outputs['Attribute.004'])

        node = tree.nodes['Simulation Input ZoneIterate2']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationInput")
        self.assert_value_socket(node.inputs['Attribute'])
        self.assert_value_socket(node.inputs['Attribute.001'])
        self.assert_value_socket(node.inputs['Attribute.002'])
        self.assert_value_socket(node.inputs['Attribute.003'])
        self.assert_value_or_field_socket(node.inputs['Attribute.004'])
        self.assert_value_socket(node.outputs['Attribute'])
        self.assert_value_socket(node.outputs['Attribute.001'])
        self.assert_value_socket(node.outputs['Attribute.002'])
        self.assert_value_socket(node.outputs['Attribute.003'])
        self.assert_value_or_field_socket(node.outputs['Attribute.004'])

        node = tree.nodes['Simulation Output ZoneIterate2']
        self.assertEqual(node.bl_idname, "GeometryNodeSimulationOutput")
        self.assert_value_socket(node.inputs['Attribute'])
        self.assert_value_socket(node.inputs['Attribute.001'])
        self.assert_value_socket(node.inputs['Attribute.002'])
        self.assert_value_socket(node.inputs['Attribute.003'])
        self.assert_value_or_field_socket(node.inputs['Attribute.004'])
        self.assert_value_socket(node.outputs['Attribute'])
        self.assert_value_socket(node.outputs['Attribute.001'])
        self.assert_value_socket(node.outputs['Attribute.002'])
        self.assert_value_socket(node.outputs['Attribute.003'])
        self.assert_value_or_field_socket(node.outputs['Attribute.004'])


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
