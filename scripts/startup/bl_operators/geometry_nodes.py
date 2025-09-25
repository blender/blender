# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import BoolProperty

from bpy.app.translations import pgettext_data as data_


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
        group.is_type_pointcloud = True
    elif ob_type == 'GREASEPENCIL':
        group.is_type_grease_pencil = True
    else:
        group.is_type_mesh = True

    mode = ob.mode if ob else 'OBJECT'
    if mode in {'SCULPT', 'SCULPT_CURVES', 'SCULPT_GREASE_PENCIL'}:
        group.is_mode_sculpt = True
    elif mode == 'PAINT_GREASE_PENCIL':
        group.is_mode_paint = True
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
            return None
        modifier = ob.modifiers.active
    if modifier is None or modifier.type != 'NODES':
        return None
    return modifier


def edit_geometry_nodes_modifier_poll(context):
    modifier = get_context_modifier(context)
    if modifier is None:
        return False
    return modifier.id_data.is_editable


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


def create_wrapper_group(operator, modifier, old_group):
    wrapper_name = old_group.name + ".wrapper"
    group = bpy.data.node_groups.new(wrapper_name, 'GeometryNodeTree')
    group.interface.new_socket(data_("Geometry"), in_out='OUTPUT', socket_type='NodeSocketGeometry')
    group.is_modifier = True

    first_geometry_input = next(
        (
            item for item in old_group.interface.items_tree if item.item_type == 'SOCKET' and
            item.in_out == 'INPUT' and
            item.bl_socket_idname == 'NodeSocketGeometry'
        ),
        None,
    )
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
            # Special case for menu sockets: the modifier property is just the int
            # value, which must be converted to the enum identifier to set the new
            # interface default value. Use the RNA definition of the modifier property
            # UI to get that identifier.
            if input_socket.socket_type == 'NodeSocketMenu':
                default_value_int = modifier[identifier]
                menu_enum_items = modifier.id_properties_ui(identifier).as_dict()['items']
                # Tuples have same order as in bpy.props.EnumProperty: (identifier, name, description, icon, number).
                # In the case of an unconnected menu socket there will be one valid "DUMMY" item only.
                if len(menu_enum_items) > 1:
                    default_value_enum_item = next(item for item in menu_enum_items if item[4] == default_value_int)
                    group_node_input.default_value = default_value_enum_item[0]
            else:
                group_node_input.default_value = modifier[identifier]

    if first_geometry_input:
        group.links.new(
            group_input_node.outputs[0],
            get_socket_with_identifier(group_node.inputs, first_geometry_input.identifier),
        )

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
            operator.report({'WARNING'}, "Node group must have a geometry output")
            return None
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
            new_group = create_wrapper_group(self, modifier, old_group)
            if new_group:
                modifier.node_group = new_group

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
        return space and space.type == 'NODE_EDITOR' and space.node_tree_sub_type == 'TOOL'

    def execute(self, context):
        group = geometry_node_group_empty_tool_new(context)
        context.space_data.selected_node_group = group
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
        if not space or space.type != 'NODE_EDITOR' or not space.edit_tree or not space.edit_tree.is_editable:
            return False
        if cls.get_node(context) is None:
            return False
        return True


classes = (
    NewGeometryNodesModifier,
    NewGeometryNodeTreeAssign,
    NewGeometryNodeGroupTool,
    MoveModifierToNodes,
)
