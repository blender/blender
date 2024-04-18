# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import IntProperty, BoolProperty

from bpy.app.translations import pgettext_data as data_

from bpy.props import (
    EnumProperty,
)


def add_empty_geometry_node_group(name):
    group = bpy.data.node_groups.new(name, 'GeometryNodeTree')

    group.interface.new_socket(data_("Geometry"), in_out='INPUT', socket_type='NodeSocketGeometry')
    input_node = group.nodes.new('NodeGroupInput')
    input_node.select = False
    input_node.location.x = -200 - input_node.width

    group.interface.new_socket(data_("Geometry"), in_out='OUTPUT', socket_type='NodeSocketGeometry')
    output_node = group.nodes.new('NodeGroupOutput')
    output_node.is_active_output = True
    output_node.select = False
    output_node.location.x = 200

    return group


def geometry_node_group_empty_new(name):
    group = add_empty_geometry_node_group(name)
    group.links.new(group.nodes[data_("Group Input")].outputs[0], group.nodes[data_("Group Output")].inputs[0])
    return group


def geometry_node_group_empty_modifier_new(name):
    group = geometry_node_group_empty_new(data_("Geometry Nodes"))
    group.is_modifier = True
    return group


def geometry_node_group_empty_tool_new(context):
    group = geometry_node_group_empty_new(data_("Tool"))
    # Node tools have fake users by default, otherwise Blender will delete them since they have no users.
    group.use_fake_user = True
    group.is_tool = True

    ob = context.object
    ob_type = ob.type if ob else 'MESH'
    if ob_type == 'CURVES':
        group.is_type_curve = True
    elif ob_type == 'POINTCLOUD':
        group.is_type_point_cloud = True
    else:
        group.is_type_mesh = True

    mode = ob.mode if ob else 'OBJECT'
    if mode in {'SCULPT', 'SCULPT_CURVES'}:
        group.is_mode_sculpt = True
    elif mode == 'EDIT':
        group.is_mode_edit = True
    else:
        group.is_mode_object = True

    return group


def geometry_modifier_poll(context):
    ob = context.object

    # Test object support for geometry node modifier
    if not ob or ob.type not in {'MESH', 'POINTCLOUD', 'VOLUME', 'CURVE', 'FONT', 'CURVES', 'GREASEPENCIL'}:
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
        return 'INT'
    elif idname.startswith("NodeSocketColor"):
        return 'FLOAT_COLOR'
    elif idname.startswith("NodeSocketVector"):
        return 'FLOAT_VECTOR'
    elif idname.startswith("NodeSocketBool"):
        return 'BOOLEAN'
    elif idname.startswith("NodeSocketFloat"):
        return 'FLOAT'
    raise ValueError("Unsupported socket type")


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

def create_wrapper_group(modifier, old_group):
    wrapper_name = old_group.name + ".wrapper"
    group = bpy.data.node_groups.new(wrapper_name, 'GeometryNodeTree')
    group.interface.new_socket(data_("Geometry"), in_out='OUTPUT', socket_type='NodeSocketGeometry')
    group.is_modifier = True

    first_geometry_input = next((item for item in old_group.interface.items_tree if item.item_type == 'SOCKET' and
                                item.in_out == 'INPUT' and
                                item.bl_socket_idname == 'NodeSocketGeometry'), None)
    if first_geometry_input:
        group.interface.new_socket(data_("Geometry"), in_out='INPUT', socket_type='NodeSocketGeometry')
        group_input_node = group.nodes.new('NodeGroupInput')
        group_input_node.location.x = -200 - group_input_node.width
        group_input_node.select = False

    group_output_node = group.nodes.new('NodeGroupOutput')
    group_output_node.is_active_output = True
    group_output_node.location.x = 200
    group_output_node.select = False

    group_node = group.nodes.new("GeometryNodeGroup")
    group_node.node_tree = old_group
    group_node.update()

    # Copy default values for inputs and create named attribute input nodes.
    input_nodes = []
    for input_socket in old_group.interface.items_tree:
        if input_socket.item_type != 'SOCKET' or (input_socket.in_out not in {'INPUT', 'BOTH'}):
            continue
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

    if first_geometry_input:
        group.links.new(group_input_node.outputs[0],
                        get_socket_with_identifier(group_node.inputs, first_geometry_input.identifier))

        # Adjust locations of named attribute input nodes and group input node to make some space.
        if input_nodes:
            for i, node in enumerate(input_nodes):
                node.location.x = -175
                node.location.y = i * -50
            group_input_node.location.x = -350

    # Connect outputs to store named attribute nodes to replace modifier attribute outputs.
    store_nodes = []
    first_geometry_output = None
    for output_socket in old_group.interface.items_tree:
        if output_socket.item_type != 'SOCKET' or (output_socket.in_out not in {'OUTPUT', 'BOTH'}):
            continue
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
            self.report({'WARNING'}, "Node group must have a geometry output")
            return {'CANCELLED'}
        group.links.new(first_geometry_output, group_output_node.inputs[data_("Geometry")])

    return group


class MoveModifierToNodes(Operator):
    """Move inputs and outputs from in the modifier to a new node group"""

    bl_idname = "object.geometry_nodes_move_to_nodes"
    bl_label = "Move to Nodes"
    bl_options = {'REGISTER', 'UNDO'}

    use_selected_objects: BoolProperty(
        name="Selected Objects",
        description="Affect all selected objects instead of just the active object",
    )

    @classmethod
    def poll(cls, context):
        return edit_geometry_nodes_modifier_poll(context)

    def invoke(self, context, event):
        if event.alt:
            self.use_selected_objects = True
        return self.execute(context)

    def execute(self, context):
        active_modifier = get_context_modifier(context)
        if not active_modifier:
            return {'CANCELLED'}
        modifier_name = active_modifier.name

        objects = []
        if self.use_selected_objects:
            objects = context.selected_editable_objects
        else:
            objects = [context.object]

        for ob in objects:
            modifier = ob.modifiers[modifier_name]
            if not modifier:
                continue
            old_group = modifier.node_group
            if not old_group:
                continue
            modifier.node_group = create_wrapper_group(modifier, old_group)

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

        group = geometry_node_group_empty_modifier_new(data_("Geometry Nodes"))
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
        modifier = get_context_modifier(context)
        if not modifier:
            return {'CANCELLED'}
        group = geometry_node_group_empty_modifier_new(data_("Geometry Nodes"))
        modifier.node_group = group

        return {'FINISHED'}


class NewGeometryNodeGroupTool(Operator):
    """Create a new geometry node group for a tool"""
    bl_idname = "node.new_geometry_node_group_tool"
    bl_label = "New Geometry Node Tool Group"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space and space.type == 'NODE_EDITOR' and space.geometry_nodes_type == 'TOOL'

    def execute(self, context):
        group = geometry_node_group_empty_tool_new(context)
        context.space_data.geometry_nodes_tool_tree = group
        return {'FINISHED'}


class ZoneOperator:
    @classmethod
    def get_node(cls, context):
        node = context.active_node
        if node is None:
            return None
        if node.bl_idname == cls.output_node_type:
            return node
        if node.bl_idname == cls.input_node_type:
            return node.paired_output

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # Needs active node editor and a tree.
        if not space or space.type != 'NODE_EDITOR' or not space.edit_tree or space.edit_tree.library:
            return False
        if cls.get_node(context) is None:
            return False
        return True


class NodeOperator:
    @classmethod
    def get_node(cls, context):
        node = context.active_node
        if node is None:
            return None
        if node.bl_idname == cls.node_type:
            return node

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # Needs active node editor and a tree.
        if not space or space.type != 'NODE_EDITOR' or not space.edit_tree or space.edit_tree.library:
            return False
        node = cls.get_node(context)
        if node is None:
            return False
        return True


class SocketItemAddOperator:
    items_name = None
    active_index_name = None
    default_socket_type = 'GEOMETRY'

    def execute(self, context):
        node = self.get_node(context)
        items = getattr(node, self.items_name)
        # Remember index to move the item.
        old_active_index = getattr(node, self.active_index_name)
        if 0 <= old_active_index < len(items):
            old_active_item = items[old_active_index]
            dst_index = old_active_index + 1
            dst_type = old_active_item.socket_type
            dst_name = old_active_item.name
        else:
            dst_index = len(items)
            dst_type = self.default_socket_type
            # Empty name so it is based on the type.
            dst_name = ""
        items.new(dst_type, dst_name)
        items.move(len(items) - 1, dst_index)
        setattr(node, self.active_index_name, dst_index)
        return {'FINISHED'}


class SocketItemRemoveOperator:
    items_name = None
    active_index_name = None

    def execute(self, context):
        node = self.get_node(context)
        items = getattr(node, self.items_name)
        old_active_index = getattr(node, self.active_index_name)

        if 0 <= old_active_index < len(items):
            items.remove(items[old_active_index])

        return {'FINISHED'}


class SocketMoveItemOperator:
    items_name = None
    active_index_name = None

    direction: EnumProperty(
        name="Direction",
        items=[('UP', "Up", ""), ('DOWN', "Down", "")],
        default='UP',
    )

    def execute(self, context):
        node = self.get_node(context)
        items = getattr(node, self.items_name)
        old_active_index = getattr(node, self.active_index_name)

        if self.direction == 'UP' and old_active_index > 0:
            items.move(old_active_index, old_active_index - 1)
            setattr(node, self.active_index_name, old_active_index - 1)
        elif self.direction == 'DOWN' and old_active_index < len(items) - 1:
            items.move(old_active_index, old_active_index + 1)
            setattr(node, self.active_index_name, old_active_index + 1)

        return {'FINISHED'}


class SimulationZoneOperator(ZoneOperator):
    input_node_type = 'GeometryNodeSimulationInput'
    output_node_type = 'GeometryNodeSimulationOutput'

    items_name = "state_items"
    active_index_name = "active_index"


class SimulationZoneItemAddOperator(SimulationZoneOperator, SocketItemAddOperator, Operator):
    """Add a state item to the simulation zone"""
    bl_idname = "node.simulation_zone_item_add"
    bl_label = "Add State Item"
    bl_options = {'REGISTER', 'UNDO'}


class SimulationZoneItemRemoveOperator(SimulationZoneOperator, SocketItemRemoveOperator, Operator):
    """Remove a state item from the simulation zone"""
    bl_idname = "node.simulation_zone_item_remove"
    bl_label = "Remove State Item"
    bl_options = {'REGISTER', 'UNDO'}


class SimulationZoneItemMoveOperator(SimulationZoneOperator, SocketMoveItemOperator, Operator):
    """Move a simulation state item up or down in the list"""
    bl_idname = "node.simulation_zone_item_move"
    bl_label = "Move State Item"
    bl_options = {'REGISTER', 'UNDO'}


class RepeatZoneOperator(ZoneOperator):
    input_node_type = 'GeometryNodeRepeatInput'
    output_node_type = 'GeometryNodeRepeatOutput'

    items_name = "repeat_items"
    active_index_name = "active_index"


class RepeatZoneItemAddOperator(RepeatZoneOperator, SocketItemAddOperator, Operator):
    """Add a repeat item to the repeat zone"""
    bl_idname = "node.repeat_zone_item_add"
    bl_label = "Add Repeat Item"
    bl_options = {'REGISTER', 'UNDO'}


class RepeatZoneItemRemoveOperator(RepeatZoneOperator, SocketItemRemoveOperator, Operator):
    """Remove a repeat item from the repeat zone"""
    bl_idname = "node.repeat_zone_item_remove"
    bl_label = "Remove Repeat Item"
    bl_options = {'REGISTER', 'UNDO'}


class RepeatZoneItemMoveOperator(RepeatZoneOperator, SocketMoveItemOperator, Operator):
    """Move a repeat item up or down in the list"""
    bl_idname = "node.repeat_zone_item_move"
    bl_label = "Move Repeat Item"
    bl_options = {'REGISTER', 'UNDO'}


class BakeNodeOperator(NodeOperator):
    node_type = 'GeometryNodeBake'

    items_name = "bake_items"
    active_index_name = "active_index"


class BakeNodeItemAddOperator(BakeNodeOperator, SocketItemAddOperator, Operator):
    """Add a bake item to the bake node"""
    bl_idname = "node.bake_node_item_add"
    bl_label = "Add Bake Item"
    bl_options = {'REGISTER', 'UNDO'}


class BakeNodeItemRemoveOperator(BakeNodeOperator, SocketItemRemoveOperator, Operator):
    """Remove a bake item from the bake node"""
    bl_idname = "node.bake_node_item_remove"
    bl_label = "Remove Bake Item"
    bl_options = {'REGISTER', 'UNDO'}


class BakeNodeItemMoveOperator(BakeNodeOperator, SocketMoveItemOperator, Operator):
    """Move a bake item up or down in the list"""
    bl_idname = "node.bake_node_item_move"
    bl_label = "Move Bake Item"
    bl_options = {'REGISTER', 'UNDO'}


def _editable_tree_with_active_node_type(context, node_type):
    space = context.space_data
    # Needs active node editor and a tree.
    if not space or space.type != 'NODE_EDITOR' or not space.edit_tree or space.edit_tree.library:
        return False
    node = context.active_node
    if node is None or node.bl_idname != node_type:
        return False
    return True


class IndexSwitchItemAddOperator(Operator):
    """Add an item to the index switch"""
    bl_idname = "node.index_switch_item_add"
    bl_label = "Add Item"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return _editable_tree_with_active_node_type(context, 'GeometryNodeIndexSwitch')

    def execute(self, context):
        node = context.active_node
        node.index_switch_items.new()
        return {'FINISHED'}


class IndexSwitchItemRemoveOperator(Operator):
    """Remove an item from the index switch"""
    bl_idname = "node.index_switch_item_remove"
    bl_label = "Remove Item"
    bl_options = {'REGISTER', 'UNDO'}

    index: IntProperty(
        name="Index",
        description="Index of item to remove",
    )

    @classmethod
    def poll(cls, context):
        return _editable_tree_with_active_node_type(context, 'GeometryNodeIndexSwitch')

    def execute(self, context):
        node = context.active_node
        items = node.index_switch_items
        items.remove(items[self.index])
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
    BakeNodeItemAddOperator,
    BakeNodeItemRemoveOperator,
    BakeNodeItemMoveOperator,
    IndexSwitchItemAddOperator,
    IndexSwitchItemRemoveOperator,
)
