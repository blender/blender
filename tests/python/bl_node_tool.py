# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import bpy


def create_object_mode_mesh_tool_tree(name, tool_idname):
    tree = bpy.data.node_groups.new(name, "GeometryNodeTree")
    tree.is_tool = True
    tree.is_mode_object = True
    tree.is_type_mesh = True
    tree.node_tool_idname = tool_idname
    return tree


def create_join_geometry_tool(tool_idname):
    """
    Create a geometry node group set up as an object-mode tool.
    Returns the node tree and the identifier of the Object input socket.
    """
    tree = create_object_mode_mesh_tool_tree("TestNodeTool", tool_idname)

    tree.interface.new_socket("Geometry", in_out='INPUT', socket_type='NodeSocketGeometry')
    obj_socket = tree.interface.new_socket("Object", in_out='INPUT', socket_type='NodeSocketObject')
    tree.interface.new_socket("Geometry", in_out='OUTPUT', socket_type='NodeSocketGeometry')

    group_input = tree.nodes.new("NodeGroupInput")
    group_output = tree.nodes.new("NodeGroupOutput")
    obj_info = tree.nodes.new("GeometryNodeObjectInfo")
    join_geo = tree.nodes.new("GeometryNodeJoinGeometry")

    tree.links.new(group_input.outputs["Geometry"], join_geo.inputs["Geometry"])
    tree.links.new(group_input.outputs["Object"], obj_info.inputs["Object"])
    tree.links.new(obj_info.outputs["Geometry"], join_geo.inputs["Geometry"])
    tree.links.new(join_geo.outputs["Geometry"], group_output.inputs["Geometry"])

    return tree, obj_socket.identifier


class TestNodeTool(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)

    def test_join_geometry_with_object_input(self):
        from bpy.types import WindowManager
        # Add a cube (the active object the tool will run on).
        bpy.ops.mesh.primitive_cube_add()
        cube = bpy.context.active_object
        cube_vertex_count = len(cube.data.vertices)

        # Add a monkey/Suzanne (the tool's object input, joined into the cube).
        bpy.ops.mesh.primitive_monkey_add()
        monkey = bpy.context.active_object
        monkey_vertex_count = len(monkey.data.vertices)

        # Make the cube the active, selected object.
        bpy.context.view_layer.objects.active = cube
        for obj in bpy.context.scene.objects:
            obj.select_set(obj == cube)

        _tree, obj_input_identifier = create_join_geometry_tool("geometry.test_node_tool_join_geometry")

        WindowManager.register_node_group_operators()

        bpy.ops.geometry.test_node_tool_join_geometry(
            'EXEC_DEFAULT',
            inputs={obj_input_identifier: {"value": monkey.name}},
        )

        # The cube's mesh should now contain geometry from both the cube and the
        # monkey, so the vertex count should be the sum of both.
        expected_vertex_count = cube_vertex_count + monkey_vertex_count
        self.assertEqual(len(cube.data.vertices), expected_vertex_count)

    def test_string_inputs_store_named_attribute(self):
        from bpy.types import WindowManager

        bpy.ops.mesh.primitive_cube_add()
        cube = bpy.context.active_object

        tree = create_object_mode_mesh_tool_tree(
            "TestNodeToolStringInputs", "geometry.test_node_tool_string_inputs"
        )
        tree.interface.new_socket("Geometry", in_out='INPUT', socket_type='NodeSocketGeometry')
        str_default_socket = tree.interface.new_socket(
            "Name With Default", in_out='INPUT', socket_type='NodeSocketString')
        str_default_socket.default_value = "attr_with_default"
        str_no_default_socket = tree.interface.new_socket(
            "Name Without Default", in_out='INPUT', socket_type='NodeSocketString')
        tree.interface.new_socket("Geometry", in_out='OUTPUT', socket_type='NodeSocketGeometry')

        group_input = tree.nodes.new("NodeGroupInput")
        group_output = tree.nodes.new("NodeGroupOutput")
        store_default = tree.nodes.new("GeometryNodeStoreNamedAttribute")
        store_no_default = tree.nodes.new("GeometryNodeStoreNamedAttribute")

        tree.links.new(group_input.outputs["Geometry"], store_default.inputs["Geometry"])
        tree.links.new(store_default.outputs["Geometry"], store_no_default.inputs["Geometry"])
        tree.links.new(store_no_default.outputs["Geometry"], group_output.inputs["Geometry"])
        tree.links.new(group_input.outputs["Name With Default"], store_default.inputs["Name"])
        tree.links.new(group_input.outputs["Name Without Default"], store_no_default.inputs["Name"])

        WindowManager.register_node_group_operators()

        # The string with a default value is left at its default ("attr_with_default").
        # The string without a default is explicitly set when calling the tool.
        bpy.ops.geometry.test_node_tool_string_inputs(
            'EXEC_DEFAULT',
            inputs={str_no_default_socket.identifier: {"value": "attr_without_default"}},
        )

        attribute_names = {attr.name for attr in cube.data.attributes}
        self.assertIn("attr_with_default", attribute_names)
        self.assertIn("attr_without_default", attribute_names)

    def test_menu_input(self):
        from bpy.types import WindowManager

        bpy.ops.mesh.primitive_cube_add()
        cube = bpy.context.active_object

        tree = create_object_mode_mesh_tool_tree(
            "TestNodeToolMenu", "geometry.test_node_tool_menu"
        )
        tree.interface.new_socket("Geometry", in_out='INPUT', socket_type='NodeSocketGeometry')
        mode_socket = tree.interface.new_socket(
            "Mode", in_out='INPUT', socket_type='NodeSocketMenu')
        tree.interface.new_socket("Geometry", in_out='OUTPUT', socket_type='NodeSocketGeometry')

        group_input = tree.nodes.new("NodeGroupInput")
        group_output = tree.nodes.new("NodeGroupOutput")
        menu_switch = tree.nodes.new("GeometryNodeMenuSwitch")
        menu_switch.data_type = 'VECTOR'
        transform = tree.nodes.new("GeometryNodeTransform")

        # The menu switch starts with default items "A" and "B". Rename and add a third so the items become X, Y, Z.
        menu_switch.enum_items[0].name = "X"
        menu_switch.enum_items[1].name = "Y"
        menu_switch.enum_items.new("Z")

        menu_switch.inputs["X"].default_value = (10.0, 0.0, 0.0)
        menu_switch.inputs["Y"].default_value = (0.0, 10.0, 0.0)
        menu_switch.inputs["Z"].default_value = (0.0, 0.0, 10.0)

        tree.links.new(group_input.outputs["Geometry"], transform.inputs["Geometry"])
        tree.links.new(group_input.outputs["Mode"], menu_switch.inputs["Menu"])
        tree.links.new(menu_switch.outputs["Output"], transform.inputs["Translation"])
        tree.links.new(transform.outputs["Geometry"], group_output.inputs["Geometry"])

        WindowManager.register_node_group_operators()

        bpy.ops.geometry.test_node_tool_menu(
            'EXEC_DEFAULT',
            inputs={mode_socket.identifier: {"value": "Y"}},
        )

        verts = cube.data.vertices
        n = len(verts)
        center_x = sum(v.co.x for v in verts) / n
        center_y = sum(v.co.y for v in verts) / n
        center_z = sum(v.co.z for v in verts) / n
        self.assertAlmostEqual(center_x, 0.0, places=4)
        self.assertAlmostEqual(center_y, 10.0, places=4)
        self.assertAlmostEqual(center_z, 0.0, places=4)

    def test_image_input_sample_texture(self):
        from bpy.types import WindowManager

        # Build a 32x32 image: left half white, right half black.
        size = 32
        image = bpy.data.images.new("TestImage", width=size, height=size)
        white = [1.0, 1.0, 1.0, 1.0]
        black = [0.0, 0.0, 0.0, 1.0]
        row = (white * (size // 2)) + (black * (size // 2))
        pixels = row * size
        image.pixels.foreach_set(pixels)
        image.update()

        bpy.ops.mesh.primitive_plane_add()
        plane = bpy.context.active_object

        tree = create_object_mode_mesh_tool_tree(
            "TestNodeToolImage", "geometry.test_node_tool_image"
        )
        tree.interface.new_socket("Geometry", in_out='INPUT', socket_type='NodeSocketGeometry')
        image_socket = tree.interface.new_socket(
            "Image", in_out='INPUT', socket_type='NodeSocketImage')
        tree.interface.new_socket("Geometry", in_out='OUTPUT', socket_type='NodeSocketGeometry')

        group_input = tree.nodes.new("NodeGroupInput")
        group_output = tree.nodes.new("NodeGroupOutput")

        uv_attr = tree.nodes.new("GeometryNodeInputNamedAttribute")
        uv_attr.data_type = 'FLOAT_VECTOR'
        uv_attr.inputs["Name"].default_value = "UVMap"

        image_texture = tree.nodes.new("GeometryNodeImageTexture")
        # Avoid edge interpolation/wrap effects so the leftmost vertices sample pure white.
        image_texture.interpolation = 'Closest'
        image_texture.extension = 'EXTEND'

        store_attr = tree.nodes.new("GeometryNodeStoreNamedAttribute")
        store_attr.data_type = 'FLOAT'
        store_attr.inputs["Name"].default_value = "sampled_value"

        tree.links.new(group_input.outputs["Geometry"], store_attr.inputs["Geometry"])
        tree.links.new(group_input.outputs["Image"], image_texture.inputs["Image"])
        tree.links.new(uv_attr.outputs["Attribute"], image_texture.inputs["Vector"])
        tree.links.new(image_texture.outputs["Color"], store_attr.inputs["Value"])
        tree.links.new(store_attr.outputs["Geometry"], group_output.inputs["Geometry"])

        WindowManager.register_node_group_operators()

        bpy.ops.geometry.test_node_tool_image(
            'EXEC_DEFAULT',
            inputs={image_socket.identifier: {"value": image.name}},
        )

        value_attr = plane.data.attributes["sampled_value"]
        left_vertex_indices = [i for i, v in enumerate(plane.data.vertices) if v.co.x < 0.0]
        self.assertGreater(len(left_vertex_indices), 0)
        for i in left_vertex_indices:
            self.assertAlmostEqual(value_attr.data[i].value, 1.0, places=2)


if __name__ == "__main__":
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
