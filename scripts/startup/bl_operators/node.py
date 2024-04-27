# SPDX-FileCopyrightText: 2012-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import bpy
from bpy.types import (
    FileHandler,
    Operator,
    PropertyGroup,
)
from bpy.props import (
    BoolProperty,
    CollectionProperty,
    EnumProperty,
    FloatVectorProperty,
    StringProperty,
)
from mathutils import (
    Vector,
)

from bpy.app.translations import (
    pgettext_tip as tip_,
    pgettext_rpt as rpt_,
    pgettext_data as data_,
)


class NodeSetting(PropertyGroup):
    value: StringProperty(
        name="Value",
        description="Python expression to be evaluated "
        "as the initial node setting",
        default="",
    )


# Base class for node "Add" operators.
class NodeAddOperator:

    use_transform: BoolProperty(
        name="Use Transform",
        description="Start transform operator after inserting the node",
        default=False,
    )
    settings: CollectionProperty(
        name="Settings",
        description="Settings to be applied on the newly created node",
        type=NodeSetting,
        options={'SKIP_SAVE'},
    )

    @staticmethod
    def store_mouse_cursor(context, event):
        space = context.space_data
        tree = space.edit_tree

        # convert mouse position to the View2D for later node placement
        if context.region.type == 'WINDOW':
            # convert mouse position to the View2D for later node placement
            space.cursor_location_from_region(event.mouse_region_x, event.mouse_region_y)
        else:
            space.cursor_location = tree.view_center

    # Deselect all nodes in the tree.
    @staticmethod
    def deselect_nodes(context):
        space = context.space_data
        tree = space.edit_tree
        for n in tree.nodes:
            n.select = False

    def create_node(self, context, node_type):
        space = context.space_data
        tree = space.edit_tree

        try:
            node = tree.nodes.new(type=node_type)
        except RuntimeError as ex:
            self.report({'ERROR'}, str(ex))
            return None

        for setting in self.settings:
            # XXX catch exceptions here?
            value = eval(setting.value)
            node_data = node
            node_attr_name = setting.name

            # Support path to nested data.
            if '.' in node_attr_name:
                node_data_path, node_attr_name = node_attr_name.rsplit(".", 1)
                node_data = node.path_resolve(node_data_path)

            try:
                setattr(node_data, node_attr_name, value)
            except AttributeError as ex:
                self.report(
                    {'ERROR_INVALID_INPUT'},
                    rpt_("Node has no attribute {:s}").format(setting.name))
                print(str(ex))
                # Continue despite invalid attribute

        node.select = True
        tree.nodes.active = node
        node.location = space.cursor_location
        return node

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # needs active node editor and a tree to add nodes to
        return (space and (space.type == 'NODE_EDITOR') and
                space.edit_tree and not space.edit_tree.library)

    # Default invoke stores the mouse position to place the node correctly
    # and optionally invokes the transform operator
    def invoke(self, context, event):
        self.store_mouse_cursor(context, event)
        result = self.execute(context)

        if self.use_transform and ('FINISHED' in result):
            # removes the node again if transform is canceled
            bpy.ops.node.translate_attach_remove_on_cancel('INVOKE_DEFAULT')

        return result


# Simple basic operator for adding a node.
class NODE_OT_add_node(NodeAddOperator, Operator):
    """Add a node to the active tree"""
    bl_idname = "node.add_node"
    bl_label = "Add Node"
    bl_options = {'REGISTER', 'UNDO'}

    type: StringProperty(
        name="Node Type",
        description="Node type",
    )

    # Default execute simply adds a node.
    def execute(self, context):
        if self.properties.is_property_set("type"):
            self.deselect_nodes(context)
            self.create_node(context, self.type)
            return {'FINISHED'}
        else:
            return {'CANCELLED'}

    @classmethod
    def description(cls, _context, properties):
        nodetype = properties["type"]
        bl_rna = bpy.types.Node.bl_rna_get_subclass(nodetype)
        if bl_rna is not None:
            return tip_(bl_rna.description)
        else:
            return ""


class NodeAddZoneOperator(NodeAddOperator):
    offset: FloatVectorProperty(
        name="Offset",
        description="Offset of nodes from the cursor when added",
        size=2,
        default=(150, 0),
    )

    def execute(self, context):
        space = context.space_data
        tree = space.edit_tree

        self.deselect_nodes(context)
        input_node = self.create_node(context, self.input_node_type)
        output_node = self.create_node(context, self.output_node_type)
        if input_node is None or output_node is None:
            return {'CANCELLED'}

        # Simulation input must be paired with the output.
        input_node.pair_with_output(output_node)

        input_node.location -= Vector(self.offset)
        output_node.location += Vector(self.offset)

        # Connect geometry sockets by default.
        # Get the sockets by their types, because the name is not guaranteed due to i18n.
        from_socket = next(s for s in input_node.outputs if s.type == 'GEOMETRY')
        to_socket = next(s for s in output_node.inputs if s.type == 'GEOMETRY')
        tree.links.new(to_socket, from_socket)

        return {'FINISHED'}


class NODE_OT_add_simulation_zone(NodeAddZoneOperator, Operator):
    """Add simulation zone input and output nodes to the active tree"""
    bl_idname = "node.add_simulation_zone"
    bl_label = "Add Simulation Zone"
    bl_options = {'REGISTER', 'UNDO'}

    input_node_type = "GeometryNodeSimulationInput"
    output_node_type = "GeometryNodeSimulationOutput"


class NODE_OT_add_repeat_zone(NodeAddZoneOperator, Operator):
    """Add a repeat zone that allows executing nodes a dynamic number of times"""
    bl_idname = "node.add_repeat_zone"
    bl_label = "Add Repeat Zone"
    bl_options = {'REGISTER', 'UNDO'}

    input_node_type = "GeometryNodeRepeatInput"
    output_node_type = "GeometryNodeRepeatOutput"


class NODE_OT_collapse_hide_unused_toggle(Operator):
    """Toggle collapsed nodes and hide unused sockets"""
    bl_idname = "node.collapse_hide_unused_toggle"
    bl_label = "Collapse and Hide Unused Sockets"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # needs active node editor and a tree
        return (space and (space.type == 'NODE_EDITOR') and
                (space.edit_tree and not space.edit_tree.library))

    def execute(self, context):
        space = context.space_data
        tree = space.edit_tree

        for node in tree.nodes:
            if node.select:
                hide = (not node.hide)

                node.hide = hide
                # Note: connected sockets are ignored internally
                for socket in node.inputs:
                    socket.hide = hide
                for socket in node.outputs:
                    socket.hide = hide

        return {'FINISHED'}


class NODE_OT_tree_path_parent(Operator):
    """Go to parent node tree"""
    bl_idname = "node.tree_path_parent"
    bl_label = "Parent Node Tree"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # needs active node editor and a tree
        return (space and (space.type == 'NODE_EDITOR') and len(space.path) > 1)

    def execute(self, context):
        space = context.space_data

        space.path.pop()

        return {'FINISHED'}


class NodeInterfaceOperator():
    @classmethod
    def poll(cls, context):
        space = context.space_data
        if not space or space.type != 'NODE_EDITOR' or not space.edit_tree:
            return False
        if space.edit_tree.is_embedded_data:
            return False
        return True


class NODE_OT_interface_item_new(NodeInterfaceOperator, Operator):
    '''Add a new item to the interface'''
    bl_idname = "node.interface_item_new"
    bl_label = "New Item"
    bl_options = {'REGISTER', 'UNDO'}

    item_type: EnumProperty(
        name="Item Type",
        description="Type of the item to create",
        items=(
            ('INPUT', "Input", ""),
            ('OUTPUT', "Output", ""),
            ('PANEL', "Panel", ""),
        ),
        default='INPUT',
    )

    # Returns a valid socket type for the given tree or None.
    @staticmethod
    def find_valid_socket_type(tree):
        socket_type = 'NodeSocketFloat'
        # Socket type validation function is only available for custom
        # node trees. Assume that 'NodeSocketFloat' is valid for
        # built-in node tree types.
        if not hasattr(tree, "valid_socket_type") or tree.valid_socket_type(socket_type):
            return socket_type
        # Custom nodes may not support float sockets, search all
        # registered socket subclasses.
        types_to_check = [bpy.types.NodeSocket]
        while types_to_check:
            t = types_to_check.pop()
            idname = getattr(t, "bl_idname", "")
            if tree.valid_socket_type(idname):
                return idname
            # Test all subclasses
            types_to_check.extend(t.__subclasses__())

    def execute(self, context):
        snode = context.space_data
        tree = snode.edit_tree
        interface = tree.interface

        # Remember active item and position to determine target position.
        active_item = interface.active
        active_pos = active_item.position if active_item else -1

        if self.item_type == 'INPUT':
            item = interface.new_socket("Socket", socket_type=self.find_valid_socket_type(tree), in_out='INPUT')
        elif self.item_type == 'OUTPUT':
            item = interface.new_socket("Socket", socket_type=self.find_valid_socket_type(tree), in_out='OUTPUT')
        elif self.item_type == 'PANEL':
            item = interface.new_panel("Panel")
        else:
            return {'CANCELLED'}

        if active_item:
            # Insert into active panel if possible, otherwise insert after active item.
            if active_item.item_type == 'PANEL' and item.item_type != 'PANEL':
                interface.move_to_parent(item, active_item, len(active_item.interface_items))
            else:
                interface.move_to_parent(item, active_item.parent, active_pos + 1)
        interface.active = item

        return {'FINISHED'}


class NODE_OT_interface_item_duplicate(NodeInterfaceOperator, Operator):
    '''Add a copy of the active item to the interface'''
    bl_idname = "node.interface_item_duplicate"
    bl_label = "Duplicate Item"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False

        snode = context.space_data
        tree = snode.edit_tree
        interface = tree.interface
        return interface.active is not None

    def execute(self, context):
        snode = context.space_data
        tree = snode.edit_tree
        interface = tree.interface
        item = interface.active

        if item:
            item_copy = interface.copy(item)
            interface.active = item_copy

        return {'FINISHED'}


class NODE_OT_interface_item_remove(NodeInterfaceOperator, Operator):
    '''Remove active item from the interface'''
    bl_idname = "node.interface_item_remove"
    bl_label = "Remove Item"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        snode = context.space_data
        tree = snode.edit_tree
        interface = tree.interface
        item = interface.active

        if item:
            interface.remove(item)
            interface.active_index = min(interface.active_index, len(interface.items_tree) - 1)

        return {'FINISHED'}


class NODE_OT_enum_definition_item_add(Operator):
    '''Add an enum item to the definition'''
    bl_idname = "node.enum_definition_item_add"
    bl_label = "Add Item"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        node = context.active_node
        enum_def = node.enum_definition
        item = enum_def.enum_items.new(data_("Item"))
        enum_def.active_index = enum_def.enum_items[:].index(item)
        return {'FINISHED'}


class NODE_OT_enum_definition_item_remove(Operator):
    '''Remove the selected enum item from the definition'''
    bl_idname = "node.enum_definition_item_remove"
    bl_label = "Remove Item"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        node = context.active_node
        enum_def = node.enum_definition
        item = enum_def.active_item
        if item:
            enum_def.enum_items.remove(item)
        enum_def.active_index = min(max(enum_def.active_index, 0), len(enum_def.enum_items) - 1)
        return {'FINISHED'}


class NODE_OT_enum_definition_item_move(Operator):
    '''Remove the selected enum item from the definition'''
    bl_idname = "node.enum_definition_item_move"
    bl_label = "Move Item"
    bl_options = {'REGISTER', 'UNDO'}

    direction: EnumProperty(
        name="Direction",
        description="Move up or down",
        items=[("UP", "Up", ""), ("DOWN", "Down", "")]
    )

    def execute(self, context):
        node = context.active_node
        enum_def = node.enum_definition
        index = enum_def.active_index
        if self.direction == 'UP':
            enum_def.enum_items.move(index, index - 1)
            enum_def.active_index = min(max(index - 1, 0), len(enum_def.enum_items) - 1)
        else:
            enum_def.enum_items.move(index, index + 1)
            enum_def.active_index = min(max(index + 1, 0), len(enum_def.enum_items) - 1)
        return {'FINISHED'}


class NODE_FH_image_node(FileHandler):
    bl_idname = "NODE_FH_image_node"
    bl_label = "Image node"
    bl_import_operator = "node.add_file"
    bl_file_extensions = ";".join((*bpy.path.extensions_image, *bpy.path.extensions_movie))

    @classmethod
    def poll_drop(cls, context):
        return (
            (context.area is not None) and
            (context.area.type == 'NODE_EDITOR') and
            (context.region is not None) and
            (context.region.type == 'WINDOW')
        )


classes = (
    NodeSetting,

    NODE_FH_image_node,

    NODE_OT_add_node,
    NODE_OT_add_simulation_zone,
    NODE_OT_add_repeat_zone,
    NODE_OT_collapse_hide_unused_toggle,
    NODE_OT_interface_item_new,
    NODE_OT_interface_item_duplicate,
    NODE_OT_interface_item_remove,
    NODE_OT_tree_path_parent,
    NODE_OT_enum_definition_item_add,
    NODE_OT_enum_definition_item_remove,
    NODE_OT_enum_definition_item_move,
)
