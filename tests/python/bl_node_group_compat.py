# SPDX-FileCopyrightText: 2021-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import sys
import unittest
import tempfile
import math
from dataclasses import dataclass

import bpy

args = None


base_idname = {
    "VALUE": "NodeSocketFloat",
    "INT": "NodeSocketInt",
    "BOOLEAN": "NodeSocketBool",
    "ROTATION": "NodeSocketRotation",
    "VECTOR": "NodeSocketVector",
    "RGBA": "NodeSocketColor",
    "STRING": "NodeSocketString",
    "SHADER": "NodeSocketShader",
    "OBJECT": "NodeSocketObject",
    "IMAGE": "NodeSocketImage",
    "GEOMETRY": "NodeSocketGeometry",
    "COLLECTION": "NodeSocketCollection",
    "TEXTURE": "NodeSocketTexture",
    "MATERIAL": "NodeSocketMaterial",
}


subtype_idname = {
    ("VALUE", "NONE"): "NodeSocketFloat",
    ("VALUE", "UNSIGNED"): "NodeSocketFloatUnsigned",
    ("VALUE", "PERCENTAGE"): "NodeSocketFloatPercentage",
    ("VALUE", "FACTOR"): "NodeSocketFloatFactor",
    ("VALUE", "ANGLE"): "NodeSocketFloatAngle",
    ("VALUE", "TIME"): "NodeSocketFloatTime",
    ("VALUE", "TIME_ABSOLUTE"): "NodeSocketFloatTimeAbsolute",
    ("VALUE", "DISTANCE"): "NodeSocketFloatDistance",
    ("INT", "NONE"): "NodeSocketInt",
    ("INT", "UNSIGNED"): "NodeSocketIntUnsigned",
    ("INT", "PERCENTAGE"): "NodeSocketIntPercentage",
    ("INT", "FACTOR"): "NodeSocketIntFactor",
    ("BOOLEAN", "NONE"): "NodeSocketBool",
    ("ROTATION", "NONE"): "NodeSocketRotation",
    ("VECTOR", "NONE"): "NodeSocketVector",
    ("VECTOR", "TRANSLATION"): "NodeSocketVectorTranslation",
    ("VECTOR", "DIRECTION"): "NodeSocketVectorDirection",
    ("VECTOR", "VELOCITY"): "NodeSocketVectorVelocity",
    ("VECTOR", "ACCELERATION"): "NodeSocketVectorAcceleration",
    ("VECTOR", "EULER"): "NodeSocketVectorEuler",
    ("VECTOR", "XYZ"): "NodeSocketVectorXYZ",
    ("RGBA", "NONE"): "NodeSocketColor",
    ("STRING", "NONE"): "NodeSocketString",
    ("SHADER", "NONE"): "NodeSocketShader",
    ("OBJECT", "NONE"): "NodeSocketObject",
    ("IMAGE", "NONE"): "NodeSocketImage",
    ("GEOMETRY", "NONE"): "NodeSocketGeometry",
    ("COLLECTION", "NONE"): "NodeSocketCollection",
    ("TEXTURE", "NONE"): "NodeSocketTexture",
    ("MATERIAL", "NONE"): "NodeSocketMaterial",
}


@dataclass
class SocketSpec():
    name: str
    identifier: str
    type: str
    subtype: str = 'NONE'
    hide_value: bool = False
    hide_in_modifier: bool = False
    default_value: object = None
    min_value: object = None
    max_value: object = None
    internal_links: int = 1
    external_links: int = 1

    @property
    def base_idname(self):
        return base_idname[self.type]

    @property
    def subtype_idname(self):
        return subtype_idname[(self.type, self.subtype)]


class AbstractNodeGroupInterfaceTest(unittest.TestCase):
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

    def subtype_compare(self, value, expected, subtype):
        # Rounding errors introduced at various levels, only check for roughly expected values.
        if subtype in {'ANGLE', 'EULER'}:
            # Angle values are shown in degrees in the UI, but stored as radians.
            self.assertAlmostEqual(value, math.radians(expected))
        else:
            self.assertAlmostEqual(value, expected)

    # Test properties of a node group item and associated node socket with spec data.
    def compare_group_socket_to_spec(self, item, node, spec: SocketSpec, test_links=True):
        group = item.id_data

        # Examine the interface item.
        self.assertEqual(item.name, spec.name)
        self.assertEqual(item.bl_socket_idname, spec.base_idname)
        self.assertEqual(item.identifier, spec.identifier)

        # Types that have subtypes.
        if spec.type in {'VALUE', 'INT', 'VECTOR'}:
            self.assertEqual(item.subtype, spec.subtype)

        self.assertEqual(item.hide_value, spec.hide_value)
        self.assertEqual(item.hide_in_modifier, spec.hide_in_modifier)

        if spec.type in {'VALUE', 'INT'}:
            self.subtype_compare(item.default_value, spec.default_value, spec.subtype)
            self.assertEqual(item.min_value, spec.min_value)
            self.assertEqual(item.max_value, spec.max_value)
        elif spec.type == 'VECTOR':
            self.subtype_compare(item.default_value[0], spec.default_value[0], spec.subtype)
            self.subtype_compare(item.default_value[1], spec.default_value[1], spec.subtype)
            self.subtype_compare(item.default_value[2], spec.default_value[2], spec.subtype)
            self.assertEqual(item.min_value, spec.min_value)
            self.assertEqual(item.max_value, spec.max_value)
        elif spec.type == 'RGBA':
            # Colors stored as int8 internally, enough rounding error to require fuzzy test.
            self.assertAlmostEqual(item.default_value[0], spec.default_value[0])
            self.assertAlmostEqual(item.default_value[1], spec.default_value[1])
            self.assertAlmostEqual(item.default_value[2], spec.default_value[2])
            self.assertAlmostEqual(item.default_value[3], spec.default_value[3])
        elif spec.type in {'STRING', 'BOOLEAN', 'MATERIAL', 'TEXTURE', 'OBJECT', 'COLLECTION', 'IMAGE'}:
            self.assertEqual(item.default_value, spec.default_value)
        elif spec.type in {'SHADER', 'GEOMETRY'}:
            pass
        else:
            # Add socket type testing above if this happens.
            self.fail("Socket type not supported by test")

        # Examine the node socket.
        if 'INPUT' in item.in_out:
            socket = next(s for s in node.inputs if s.identifier == spec.identifier)
            self.assertIsNotNone(socket, f"Could not find socket for group input identifier {spec.identifier}")
            self.assertEqual(socket.name, spec.name)
            self.assertEqual(socket.bl_idname, spec.subtype_idname)
            self.assertEqual(socket.type, spec.type)
            self.assertEqual(socket.hide_value, spec.hide_value)
            if test_links:
                self.assertEqual(len(socket.links), spec.external_links,
                                 f"Socket should have exactly {spec.external_links} external connections")

            input_node = next(n for n in group.nodes if n.bl_idname == 'NodeGroupInput')
            self.assertIsNotNone(input_node, "Could not find an input node in the group")
            socket = next(s for s in input_node.outputs if s.identifier == spec.identifier)
            self.assertIsNotNone(
                socket, f"Could not find group input socket for group input identifier {spec.identifier}")
            self.assertEqual(socket.name, spec.name)
            self.assertEqual(socket.bl_idname, spec.subtype_idname)
            self.assertEqual(socket.type, spec.type)
            self.assertEqual(socket.hide_value, spec.hide_value)
            if test_links:
                self.assertEqual(len(socket.links), spec.internal_links,
                                 f"Socket should have exactly {spec.internal_links} internal connections")

        if 'OUTPUT' in item.in_out:
            socket = next(s for s in node.outputs if s.identifier == spec.identifier)
            self.assertIsNotNone(socket, f"Could not find socket for group output identifier {spec.identifier}")
            self.assertEqual(socket.name, spec.name)
            self.assertEqual(socket.bl_idname, spec.subtype_idname)
            self.assertEqual(socket.type, spec.type)
            self.assertEqual(socket.hide_value, spec.hide_value)
            if test_links:
                self.assertEqual(len(socket.links), spec.external_links,
                                 f"Socket should have exactly {spec.external_links} external connections")

            output_node = next(n for n in group.nodes if n.bl_idname == 'NodeGroupOutput')
            self.assertIsNotNone(output_node, "Could not find an output node in the group")
            socket = next(s for s in output_node.inputs if s.identifier == spec.identifier)
            self.assertIsNotNone(
                socket, f"Could not find group output socket for group output identifier {spec.identifier}")
            self.assertEqual(socket.name, spec.name)
            self.assertEqual(socket.bl_idname, spec.subtype_idname)
            self.assertEqual(socket.type, spec.type)
            self.assertEqual(socket.hide_value, spec.hide_value)
            if test_links:
                self.assertEqual(len(socket.links), spec.internal_links,
                                 f"Socket should have exactly {spec.internal_links} internal connections")

    # Test node group items and associated node sockets with spec data.
    def compare_group_to_specs(self, group, node, specs, test_links=True):
        for index, spec in enumerate(specs):
            self.compare_group_socket_to_spec(group.interface.items_tree[index], node, spec, test_links=test_links)


class NodeGroupVersioning36Test(AbstractNodeGroupInterfaceTest):
    def open_file(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "nodegroup36.blend"))
        self.assertEqual(bpy.data.version, (3, 6, 11))

    def test_load_compositor_nodes(self):
        self.open_file()

        tree = bpy.data.scenes['Scene'].node_tree
        group = bpy.data.node_groups.get('NodeGroup')
        self.assertIsNotNone(group, "Compositor node group not found")
        node = tree.nodes['Group']
        self.assertEqual(node.node_tree, group, "Node group must use compositor node tree")

        # autopep8: off
        self.compare_group_to_specs(group, node, [
            SocketSpec("Output Float", "Output_9", "VALUE", hide_value=True, default_value=3.0, min_value=1.0, max_value=1.0),
            SocketSpec("Output Vector", "Output_10", "VECTOR", subtype="EULER", default_value=( 10, 20, 30), min_value=-10.0, max_value=10.0),
            SocketSpec("Output Color", "Output_11", "RGBA", default_value=(0, 1, 1, 1)),

            SocketSpec("Input Float", "Input_6", "VALUE", subtype="ANGLE", default_value=-20.0, min_value=5.0, max_value=6.0),
            SocketSpec("Input Vector", "Input_7", "VECTOR", hide_value=True, default_value=( 2, 4, 6), min_value=-4.0, max_value=100.0),
            SocketSpec("Input Color", "Input_8", "RGBA", default_value=(0.5, 0.4, 0.3, 0.2)),
        ])
        # autopep8: on

    def test_load_shader_nodes(self):
        self.open_file()

        tree = bpy.data.materials['Material'].node_tree
        group = bpy.data.node_groups.get('NodeGroup.003')
        self.assertIsNotNone(group, "Shader node group not found")
        node = tree.nodes['Group']
        self.assertEqual(node.node_tree, group, "Node group must use shader node tree")

        # autopep8: off
        self.compare_group_to_specs(group, node, [
            SocketSpec("Output Float", "Output_30", "VALUE", hide_value=True, default_value=3.0, min_value=1.0, max_value=1.0),
            SocketSpec("Output Vector", "Output_31", "VECTOR", subtype="EULER", default_value=( 10, 20, 30), min_value=-10.0, max_value=10.0),
            SocketSpec("Output Color", "Output_32", "RGBA", default_value=(0, 1, 1, 1)),
            SocketSpec("Output Shader", "Output_33", "SHADER"),

            SocketSpec("Input Float", "Input_26", "VALUE", subtype="ANGLE", default_value=-20.0, min_value=5.0, max_value=6.0),
            SocketSpec("Input Vector", "Input_27", "VECTOR", hide_value=True, default_value=( 2, 4, 6), min_value=-4.0, max_value=100.0),
            SocketSpec("Input Color", "Input_28", "RGBA", default_value=(0.5, 0.4, 0.3, 0.2)),
            SocketSpec("Input Shader", "Input_29", "SHADER"),
        ])
        # autopep8: on

    def test_load_geometry_nodes(self):
        self.open_file()

        tree = bpy.data.node_groups['Geometry Nodes']
        group = bpy.data.node_groups.get('NodeGroup.002')
        self.assertIsNotNone(group, "Geometry node group not found")
        node = tree.nodes['Group']
        self.assertEqual(node.node_tree, group, "Node group must use geometry node tree")

        # autopep8: off
        self.compare_group_to_specs(group, node, [
            SocketSpec("Output Float", "Output_7", "VALUE", hide_value=True, default_value=3.0, min_value=1.0, max_value=1.0),
            SocketSpec("Output Vector", "Output_8", "VECTOR", subtype="EULER", default_value=( 10, 20, 30), min_value=-10.0, max_value=10.0),
            SocketSpec("Output Color", "Output_9", "RGBA", default_value=(0, 1, 1, 1)),
            SocketSpec("Output String", "Output_19", "STRING", default_value=""),
            SocketSpec("Output Bool", "Output_20", "BOOLEAN", default_value=False),
            SocketSpec("Output Material", "Output_21", "MATERIAL", default_value=bpy.data.materials['TestMaterial']),
            SocketSpec("Output Int", "Output_22", "INT", default_value=0, min_value=-2147483648, max_value=2147483647),
            SocketSpec("Output Geometry", "Output_23", "GEOMETRY"),
            SocketSpec("Output Collection", "Output_24", "COLLECTION", default_value=bpy.data.collections['TestCollection']),
            SocketSpec("Output Texture", "Output_25", "TEXTURE", default_value=bpy.data.textures['TestTexture']),
            SocketSpec("Output Object", "Output_26", "OBJECT", default_value=bpy.data.objects['TestObject']),
            SocketSpec("Output Image", "Output_27", "IMAGE", default_value=bpy.data.images['TestImage']),

            SocketSpec("Input Float", "Input_4", "VALUE", subtype="ANGLE", default_value=-20.0, min_value=5.0, max_value=6.0),
            SocketSpec("Input Vector", "Input_5", "VECTOR", hide_value=True, default_value=( 2, 4, 6), min_value=-4.0, max_value=100.0),
            SocketSpec("Input Color", "Input_6", "RGBA", default_value=(0.5, 0.4, 0.3, 0.2)),
            SocketSpec("Input String", "Input_10", "STRING", default_value="hello world!"),
            SocketSpec("Input Bool", "Input_11", "BOOLEAN", default_value=True, hide_in_modifier=True),
            SocketSpec("Input Material", "Input_12", "MATERIAL", default_value=bpy.data.materials['TestMaterial']),
            SocketSpec("Input Int", "Input_13", "INT", default_value=500, min_value=200, max_value=1000),
            SocketSpec("Input Geometry", "Input_14", "GEOMETRY"),
            SocketSpec("Input Collection", "Input_15", "COLLECTION", default_value=bpy.data.collections['TestCollection']),
            SocketSpec("Input Texture", "Input_16", "TEXTURE", default_value=bpy.data.textures['TestTexture']),
            SocketSpec("Input Object", "Input_17", "OBJECT", default_value=bpy.data.objects['TestObject']),
            SocketSpec("Input Image", "Input_18", "IMAGE", default_value=bpy.data.images['TestImage']),
        ])
        # autopep8: on


class NodeGroupVersioning25Test(AbstractNodeGroupInterfaceTest):
    def open_file(self):
        bpy.ops.wm.open_mainfile(filepath=str(self.testdir / "nodegroup25.blend"))
        self.assertEqual(bpy.data.version, (2, 55, 0))

    def test_load_compositor_nodes(self):
        self.open_file()

        tree = bpy.data.scenes['Scene'].node_tree
        group = bpy.data.node_groups.get('NodeGroup.002')
        self.assertIsNotNone(group, "Compositor node group not found")
        node = tree.nodes['NodeGroup.002']
        self.assertEqual(node.node_tree, group, "Node group must use compositor node tree")

        # autopep8: off
        self.compare_group_to_specs(group, node, [
            SocketSpec("Image", "Image", "RGBA", default_value=(0, 0, 0, 1)),
            SocketSpec("Alpha", "Alpha", "VALUE", default_value=1.0, min_value=0.0, max_value=0.0),
            SocketSpec("Alpha", "Alpha.001", "VALUE", default_value=0.0, min_value=0.0, max_value=0.0),
            SocketSpec("Alpha", "Alpha.002", "VALUE", default_value=0.0, min_value=0.0, max_value=0.0),

            SocketSpec("Fac", "Fac", "VALUE", default_value=0.5, min_value=0.0, max_value=0.0),
            SocketSpec("ID value", "ID value", "VALUE", default_value=0.8, min_value=0.0, max_value=0.0),
            SocketSpec("ID value", "ID value.001", "VALUE", default_value=0.8, min_value=0.0, max_value=0.0),
        ], test_links=False)
        # autopep8: on

    def test_load_shader_nodes(self):
        self.open_file()

        tree = bpy.data.materials['Material'].node_tree
        group = bpy.data.node_groups.get('NodeGroup')
        self.assertIsNotNone(group, "Shader node group not found")
        node = tree.nodes['NodeGroup']
        self.assertEqual(node.node_tree, group, "Node group must use shader node tree")

        # autopep8: off
        self.compare_group_to_specs(group, node, [
            SocketSpec("Color", "Color", "RGBA", default_value=(0, 0, 0, 1)),
            SocketSpec("Color", "Color.001", "RGBA", default_value=(0, 0, 0, 1)),
            SocketSpec("Vector", "Vector", "VECTOR", default_value=(0, 0, 0), min_value=0.0, max_value=0.0),
            SocketSpec("Value", "Value", "VALUE", default_value=0.0, min_value=0.0, max_value=0.0),

            SocketSpec("Fac", "Fac", "VALUE", default_value=0.5, min_value=0.0, max_value=0.0),
            SocketSpec("Color1", "Color1", "RGBA", default_value=(0.5, 0.5, 0.5, 1)),
            SocketSpec("Color2", "Color2", "RGBA", default_value=(0.5, 0.5, 0.5, 1)),
            SocketSpec("Fac", "Fac.001", "VALUE", default_value=0.5, min_value=0.0, max_value=0.0),
            SocketSpec("Color1", "Color1.001", "RGBA", default_value=(0.5, 0.5, 0.5, 1)),
            SocketSpec("Color2", "Color2.001", "RGBA", default_value=(0.5, 0.5, 0.5, 1)),
            SocketSpec("Vector", "Vector", "VECTOR", default_value=(0.5, 0.5, 0.5), min_value=0.0, max_value=0.0),
            SocketSpec("Vector", "Vector.001", "VECTOR", default_value=(0.5, 0.5, 0.5), min_value=0.0, max_value=0.0),
        ], test_links=False)
        # autopep8: on


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
