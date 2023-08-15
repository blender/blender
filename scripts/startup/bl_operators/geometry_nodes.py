# SPDX-FileCopyrightText: 2020-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator

from bpy.app.translations import pgettext_data as data_

from bpy.props import (
    EnumProperty,
)


def build_default_empty_geometry_node_group(name):
    group = bpy.data.node_groups.new(name, 'GeometryNodeTree')
    group.inputs.new('NodeSocketGeometry', data_("Geometry"))
    group.outputs.new('NodeSocketGeometry', data_("Geometry"))
    input_node = group.nodes.new('NodeGroupInput')
    output_node = group.nodes.new('NodeGroupOutput')
    output_node.is_active_output = True

    input_node.select = False
    output_node.select = False

    input_node.location.x = -200 - input_node.width
    output_node.location.x = 200

    return group


def geometry_node_group_empty_new():
    group = build_default_empty_geometry_node_group(data_("Geometry Nodes"))
    group.links.new(group.nodes[data_("Group Input")].outputs[0], group.nodes[data_("Group Output")].inputs[0])
    return group


def geometry_modifier_poll(context):
    ob = context.object

    # Test object support for geometry node modifier
    if not ob or ob.type not in {'MESH', 'POINTCLOUD', 'VOLUME', 'CURVE', 'FONT', 'CURVES'}:
        return False

    return True


def get_context_modifier(context):
    # Context only has a "modifier" attribute in the modifier extra operators drop-down.
    modifier = getattr(context, "modifier", ...)
    if modifier is ...:
        ob = context.object
        if ob is None:
            return False
        modifier = ob.modifiers.active
    if modifier is None or modifier.type != 'NODES':
        return None
    return modifier


def edit_geometry_nodes_modifier_poll(context):
    return get_context_modifier(context) is not None


def socket_idname_to_attribute_type(idname):
    if idname.startswith("NodeSocketInt"):
        return "INT"
    elif idname.startswith("NodeSocketColor"):
        return "FLOAT_COLOR"
    elif idname.startswith("NodeSocketVector"):
        return "FLOAT_VECTOR"
    elif idname.startswith("NodeSocketBool"):
        return "BOOLEAN"
    elif idname.startswith("NodeSocketFloat"):
        return "FLOAT"
    raise ValueError("Unsupported socket type")
    return ""


def modifier_attribute_name_get(modifier, identifier):
    try:
        return modifier[identifier + "_attribute_name"]
    except KeyError:
        return None


def modifier_input_use_attribute(modifier, identifier):
    try:
        return modifier[identifier + "_use_attribute"] != 0
    except KeyError:
        return False


def get_socket_with_identifier(sockets, identifier):
    for socket in sockets:
        if socket.identifier == identifier:
            return socket
    return None


def get_enabled_socket_with_name(sockets, name):
    for socket in sockets:
        if socket.name == name and socket.enabled:
            return socket
    return None


class MoveModifierToNodes(Operator):
    """Move inputs and outputs from in the modifier to a new node group"""

    bl_idname = "object.geometry_nodes_move_to_nodes"
    bl_label = "Move to Nodes"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return edit_geometry_nodes_modifier_poll(context)

    def execute(self, context):
        modifier = get_context_modifier(context)
        if not modifier:
            return {'CANCELLED'}
        old_group = modifier.node_group
        if not old_group:
            return {'CANCELLED'}

        wrapper_name = old_group.name + ".wrapper"
        group = build_default_empty_geometry_node_group(wrapper_name)
        group_node = group.nodes.new("GeometryNodeGroup")
        group_node.node_tree = old_group
        group_node.update()

        group_input_node = group.nodes[data_("Group Input")]
        group_output_node = group.nodes[data_("Group Output")]

        # Copy default values for inputs and create named attribute input nodes.
        input_nodes = []
        first_geometry_input = None
        for input_socket in old_group.inputs:
            identifier = input_socket.identifier
            group_node_input = get_socket_with_identifier(group_node.inputs, identifier)
            if modifier_input_use_attribute(modifier, identifier):
                input_node = group.nodes.new("GeometryNodeInputNamedAttribute")
                input_nodes.append(input_node)
                input_node.data_type = socket_idname_to_attribute_type(input_socket.bl_socket_idname)
                attribute_name = modifier_attribute_name_get(modifier, identifier)
                input_node.inputs["Name"].default_value = attribute_name
                output_socket = get_enabled_socket_with_name(input_node.outputs, "Attribute")
                group.links.new(output_socket, group_node_input)
            elif hasattr(input_socket, "default_value"):
                group_node_input.default_value = modifier[identifier]
            elif input_socket.bl_socket_idname == 'NodeSocketGeometry':
                if not first_geometry_input:
                    first_geometry_input = group_node_input

        if not first_geometry_input:
            self.report({"WARNING"}, "Node group must have a geometry input")
            return {'CANCELLED'}
        group.links.new(group_input_node.outputs[0], first_geometry_input)

        # Adjust locations of named attribute input nodes and group input node to make some space.
        if input_nodes:
            for i, node in enumerate(input_nodes):
                node.location.x = -175
                node.location.y = i * -50
            group_input_node.location.x = -350

        # Connect outputs to store named attribute nodes to replace modifier attribute outputs.
        store_nodes = []
        first_geometry_output = None
        for output_socket in old_group.outputs:
            identifier = output_socket.identifier
            group_node_output = get_socket_with_identifier(group_node.outputs, identifier)
            attribute_name = modifier_attribute_name_get(modifier, identifier)
            if attribute_name:
                store_node = group.nodes.new("GeometryNodeStoreNamedAttribute")
                store_nodes.append(store_node)
                store_node.data_type = socket_idname_to_attribute_type(output_socket.bl_socket_idname)
                store_node.domain = output_socket.attribute_domain
                store_node.inputs["Name"].default_value = attribute_name
                input_socket = get_enabled_socket_with_name(store_node.inputs, "Value")
                group.links.new(group_node_output, input_socket)
            elif output_socket.bl_socket_idname == 'NodeSocketGeometry':
                if not first_geometry_output:
                    first_geometry_output = group_node_output

        # Adjust locations of store named attribute nodes and move group output.
        # Note that the node group has its sockets names translated, while the built-in nodes don't.
        if store_nodes:
            for i, node in enumerate(store_nodes):
                node.location.x = (i + 1) * 175
                node.location.y = 0
            group_output_node.location.x = (len(store_nodes) + 1) * 175

            group.links.new(first_geometry_output, store_nodes[0].inputs["Geometry"])
            for i in range(len(store_nodes) - 1):
                group.links.new(store_nodes[i].outputs["Geometry"], store_nodes[i + 1].inputs["Geometry"])

            group.links.new(store_nodes[-1].outputs["Geometry"], group_output_node.inputs[data_("Geometry")])
        else:
            if not first_geometry_output:
                self.report({"WARNING"}, "Node group must have a geometry output")
                return {"CANCELLED"}
            group.links.new(first_geometry_output, group_output_node.inputs[data_("Geometry")])

        modifier.node_group = group

        return {'FINISHED'}


class NewGeometryNodesModifier(Operator):
    """Create a new modifier with a new geometry node group"""

    bl_idname = "node.new_geometry_nodes_modifier"
    bl_label = "New Geometry Node Modifier"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return geometry_modifier_poll(context)

    def execute(self, context):
        ob = context.object
        modifier = ob.modifiers.new(data_("GeometryNodes"), 'NODES')
        if not modifier:
            return {'CANCELLED'}

        group = geometry_node_group_empty_new()
        modifier.node_group = group

        return {'FINISHED'}


class NewGeometryNodeTreeAssign(Operator):
    """Create a new geometry node group and assign it to the active modifier"""

    bl_idname = "node.new_geometry_node_group_assign"
    bl_label = "Assign New Geometry Node Group"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return geometry_modifier_poll(context)

    def execute(self, context):
        space = context.space_data
        if space and space.type == 'NODE_EDITOR' and space.geometry_nodes_type == 'TOOL':
            group = geometry_node_group_empty_new()
            space.node_tree = group
            return {'FINISHED'}
        else:
            modifier = get_context_modifier(context)
            if not modifier:
                return {'CANCELLED'}
            group = geometry_node_group_empty_new()
            modifier.node_group = group

        return {'FINISHED'}


class NewGeometryNodeGroupTool(Operator):
    """Create a new geometry node group for an tool"""
    bl_idname = "node.new_geometry_node_group_tool"
    bl_label = "New Geometry Node Tool Group"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.space_data.type == 'NODE_EDITOR' and context.space_data.geometry_nodes_type == 'TOOL'

    def execute(self, context):
        group = geometry_node_group_empty_new()
        group.asset_mark()
        group.is_tool = True
        context.space_data.node_tree = group
        return {'FINISHED'}


class SimulationZoneOperator:
    input_node_type = 'GeometryNodeSimulationInput'
    output_node_type = 'GeometryNodeSimulationOutput'

    @classmethod
    def get_output_node(cls, context):
        node = context.active_node
        if node.bl_idname == cls.input_node_type:
            return node.paired_output
        if node.bl_idname == cls.output_node_type:
            return node

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # Needs active node editor and a tree.
        if not space or space.type != 'NODE_EDITOR' or not space.edit_tree or space.edit_tree.library:
            return False
        node = context.active_node
        if node is None or node.bl_idname not in [cls.input_node_type, cls.output_node_type]:
            return False
        if cls.get_output_node(context) is None:
            return False
        return True


class SimulationZoneItemAddOperator(SimulationZoneOperator, Operator):
    """Add a state item to the simulation zone"""
    bl_idname = "node.simulation_zone_item_add"
    bl_label = "Add State Item"
    bl_options = {'REGISTER', 'UNDO'}

    default_socket_type = 'GEOMETRY'

    def execute(self, context):
        node = self.get_output_node(context)
        state_items = node.state_items

        # Remember index to move the item.
        if node.active_item:
            dst_index = node.active_index + 1
            dst_type = node.active_item.socket_type
            dst_name = node.active_item.name
        else:
            dst_index = len(state_items)
            dst_type = self.default_socket_type
            # Empty name so it is based on the type.
            dst_name = ""
        state_items.new(dst_type, dst_name)
        state_items.move(len(state_items) - 1, dst_index)
        node.active_index = dst_index

        return {'FINISHED'}


class SimulationZoneItemRemoveOperator(SimulationZoneOperator, Operator):
    """Remove a state item from the simulation zone"""
    bl_idname = "node.simulation_zone_item_remove"
    bl_label = "Remove State Item"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        node = self.get_output_node(context)
        state_items = node.state_items

        if node.active_item:
            state_items.remove(node.active_item)
            node.active_index = min(node.active_index, len(state_items) - 1)

        return {'FINISHED'}


class SimulationZoneItemMoveOperator(SimulationZoneOperator, Operator):
    """Move a simulation state item up or down in the list"""
    bl_idname = "node.simulation_zone_item_move"
    bl_label = "Move State Item"
    bl_options = {'REGISTER', 'UNDO'}

    direction: EnumProperty(
        name="Direction",
        items=[('UP', "Up", ""), ('DOWN', "Down", "")],
        default='UP',
    )

    def execute(self, context):
        node = self.get_output_node(context)
        state_items = node.state_items

        if self.direction == 'UP' and node.active_index > 0:
            state_items.move(node.active_index, node.active_index - 1)
            node.active_index = node.active_index - 1
        elif self.direction == 'DOWN' and node.active_index < len(state_items) - 1:
            state_items.move(node.active_index, node.active_index + 1)
            node.active_index = node.active_index + 1

        return {'FINISHED'}


class RepeatZoneOperator:
    input_node_type = 'GeometryNodeRepeatInput'
    output_node_type = 'GeometryNodeRepeatOutput'

    @classmethod
    def get_output_node(cls, context):
        node = context.active_node
        if node.bl_idname == cls.input_node_type:
            return node.paired_output
        if node.bl_idname == cls.output_node_type:
            return node
        return None

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # Needs active node editor and a tree.
        if not space or space.type != 'NODE_EDITOR' or not space.edit_tree or space.edit_tree.library:
            return False
        node = context.active_node
        if node is None or node.bl_idname not in [cls.input_node_type, cls.output_node_type]:
            return False
        if cls.get_output_node(context) is None:
            return False
        return True


class RepeatZoneItemAddOperator(RepeatZoneOperator, Operator):
    """Add a repeat item to the repeat zone"""
    bl_idname = "node.repeat_zone_item_add"
    bl_label = "Add Repeat Item"
    bl_options = {'REGISTER', 'UNDO'}

    default_socket_type = 'GEOMETRY'

    def execute(self, context):
        node = self.get_output_node(context)
        repeat_items = node.repeat_items

        # Remember index to move the item.
        if node.active_item:
            dst_index = node.active_index + 1
            dst_type = node.active_item.socket_type
            dst_name = node.active_item.name
        else:
            dst_index = len(repeat_items)
            dst_type = self.default_socket_type
            # Empty name so it is based on the type.
            dst_name = ""
        repeat_items.new(dst_type, dst_name)
        repeat_items.move(len(repeat_items) - 1, dst_index)
        node.active_index = dst_index

        return {'FINISHED'}


class RepeatZoneItemRemoveOperator(RepeatZoneOperator, Operator):
    """Remove a repeat item from the repeat zone"""
    bl_idname = "node.repeat_zone_item_remove"
    bl_label = "Remove Repeat Item"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        node = self.get_output_node(context)
        repeat_items = node.repeat_items

        if node.active_item:
            repeat_items.remove(node.active_item)
            node.active_index = min(node.active_index, len(repeat_items) - 1)

        return {'FINISHED'}


class RepeatZoneItemMoveOperator(RepeatZoneOperator, Operator):
    """Move a repeat item up or down in the list"""
    bl_idname = "node.repeat_zone_item_move"
    bl_label = "Move Repeat Item"
    bl_options = {'REGISTER', 'UNDO'}

    direction: EnumProperty(
        name="Direction",
        items=[('UP', "Up", ""), ('DOWN', "Down", "")],
        default='UP',
    )

    def execute(self, context):
        node = self.get_output_node(context)
        repeat_items = node.repeat_items

        if self.direction == 'UP' and node.active_index > 0:
            repeat_items.move(node.active_index, node.active_index - 1)
            node.active_index = node.active_index - 1
        elif self.direction == 'DOWN' and node.active_index < len(repeat_items) - 1:
            repeat_items.move(node.active_index, node.active_index + 1)
            node.active_index = node.active_index + 1

        return {'FINISHED'}


classes = (
    NewGeometryNodesModifier,
    NewGeometryNodeTreeAssign,
    NewGeometryNodeGroupTool,
    MoveModifierToNodes,
    SimulationZoneItemAddOperator,
    SimulationZoneItemRemoveOperator,
    SimulationZoneItemMoveOperator,
    RepeatZoneItemAddOperator,
    RepeatZoneItemRemoveOperator,
    RepeatZoneItemMoveOperator,
)
