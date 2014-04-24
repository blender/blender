# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

import bpy
import nodeitems_utils
from bpy.types import (Operator,
                       PropertyGroup,
                       )
from bpy.props import (BoolProperty,
                       CollectionProperty,
                       EnumProperty,
                       IntProperty,
                       StringProperty,
                       )


class NodeSetting(PropertyGroup):
    value = StringProperty(
            name="Value",
            description="Python expression to be evaluated as the initial node setting",
            default="",
            )


# Base class for node 'Add' operators
class NodeAddOperator():

    type = StringProperty(
            name="Node Type",
            description="Node type",
            )
    use_transform = BoolProperty(
            name="Use Transform",
            description="Start transform operator after inserting the node",
            default=False,
            )
    settings = CollectionProperty(
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

    # XXX explicit node_type argument is usually not necessary, but required to make search operator work:
    # add_search has to override the 'type' property since it's hardcoded in bpy_operator_wrap.c ...
    def create_node(self, context, node_type=None):
        space = context.space_data
        tree = space.edit_tree

        if node_type is None:
            node_type = self.type

        # select only the new node
        for n in tree.nodes:
            n.select = False

        node = tree.nodes.new(type=node_type)

        for setting in self.settings:
            # XXX catch exceptions here?
            value = eval(setting.value)

            try:
                setattr(node, setting.name, value)
            except AttributeError as e:
                self.report({'ERROR_INVALID_INPUT'}, "Node has no attribute " + setting.name)
                print(str(e))
                # Continue despite invalid attribute

        node.select = True
        tree.nodes.active = node
        node.location = space.cursor_location
        return node

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # needs active node editor and a tree to add nodes to
        return (space.type == 'NODE_EDITOR' and space.edit_tree and not space.edit_tree.library)

    # Default execute simply adds a node
    def execute(self, context):
        if self.properties.is_property_set("type"):
            self.create_node(context)
            return {'FINISHED'}
        else:
            return {'CANCELLED'}

    # Default invoke stores the mouse position to place the node correctly
    # and optionally invokes the transform operator
    def invoke(self, context, event):
        self.store_mouse_cursor(context, event)
        result = self.execute(context)

        if self.use_transform and ('FINISHED' in result):
            # removes the node again if transform is cancelled
            bpy.ops.transform.translate('INVOKE_DEFAULT', remove_on_cancel=True)

        return result


# Simple basic operator for adding a node
class NODE_OT_add_node(NodeAddOperator, Operator):
    '''Add a node to the active tree'''
    bl_idname = "node.add_node"
    bl_label = "Add Node"
    bl_options = {'REGISTER', 'UNDO'}


# Add a node and link it to an existing socket
class NODE_OT_add_and_link_node(NodeAddOperator, Operator):
    '''Add a node to the active tree and link to an existing socket'''
    bl_idname = "node.add_and_link_node"
    bl_label = "Add and Link Node"
    bl_options = {'REGISTER', 'UNDO'}

    link_socket_index = IntProperty(
            name="Link Socket Index",
            description="Index of the socket to link",
            )

    def execute(self, context):
        space = context.space_data
        ntree = space.edit_tree

        node = self.create_node(context)
        if not node:
            return {'CANCELLED'}

        to_socket = getattr(context, "link_to_socket", None)
        if to_socket:
            ntree.links.new(node.outputs[self.link_socket_index], to_socket)

        from_socket = getattr(context, "link_from_socket", None)
        if from_socket:
            ntree.links.new(from_socket, node.inputs[self.link_socket_index])

        return {'FINISHED'}


class NODE_OT_add_search(NodeAddOperator, Operator):
    '''Add a node to the active tree'''
    bl_idname = "node.add_search"
    bl_label = "Search and Add Node"
    bl_options = {'REGISTER', 'UNDO'}
    bl_property = "node_item"

    _enum_item_hack = []

    # Create an enum list from node items
    def node_enum_items(self, context):
        enum_items = NODE_OT_add_search._enum_item_hack
        enum_items.clear()

        for index, item in enumerate(nodeitems_utils.node_items_iter(context)):
            if isinstance(item, nodeitems_utils.NodeItem):
                nodetype = getattr(bpy.types, item.nodetype, None)
                if nodetype:
                    enum_items.append((str(index), item.label, nodetype.bl_rna.description, index))
        return enum_items

    # Look up the item based on index
    def find_node_item(self, context):
        node_item = int(self.node_item)
        for index, item in enumerate(nodeitems_utils.node_items_iter(context)):
            if index == node_item:
                return item
        return None

    node_item = EnumProperty(
            name="Node Type",
            description="Node type",
            items=node_enum_items,
            )

    def execute(self, context):
        item = self.find_node_item(context)

        # no need to keep
        self._enum_item_hack.clear()

        if item:
            # apply settings from the node item
            for setting in item.settings.items():
                ops = self.settings.add()
                ops.name = setting[0]
                ops.value = setting[1]

            self.create_node(context, item.nodetype)

            if self.use_transform:
                bpy.ops.transform.translate('INVOKE_DEFAULT', remove_on_cancel=True)

            return {'FINISHED'}
        else:
            return {'CANCELLED'}

    def invoke(self, context, event):
        self.store_mouse_cursor(context, event)
        # Delayed execution in the search popup
        context.window_manager.invoke_search_popup(self)
        return {'CANCELLED'}


class NODE_OT_collapse_hide_unused_toggle(Operator):
    '''Toggle collapsed nodes and hide unused sockets'''
    bl_idname = "node.collapse_hide_unused_toggle"
    bl_label = "Collapse and Hide Unused Sockets"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # needs active node editor and a tree
        return (space.type == 'NODE_EDITOR' and space.edit_tree and not space.edit_tree.library)

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
    '''Go to parent node tree'''
    bl_idname = "node.tree_path_parent"
    bl_label = "Parent Node Tree"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # needs active node editor and a tree
        return (space.type == 'NODE_EDITOR' and len(space.path) > 1)

    def execute(self, context):
        space = context.space_data

        space.path.pop()

        return {'FINISHED'}
