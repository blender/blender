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
from bpy.types import Operator
from bpy.props import BoolProperty, EnumProperty, StringProperty


# Base class for node 'Add' operators
class NodeAddOperator():
    @staticmethod
    def store_mouse_cursor(context, event):
        space = context.space_data
        v2d = context.region.view2d

        # convert mouse position to the View2D for later node placement
        space.cursor_location = v2d.region_to_view(event.mouse_region_x,
                                                   event.mouse_region_y)

    def create_node(self, context, node_type):
        space = context.space_data
        tree = space.edit_tree

        # select only the new node
        for n in tree.nodes:
            n.select = False

        node = tree.nodes.new(type=node_type)

        if space.use_hidden_preview:
            node.show_preview = False

        node.select = True
        tree.nodes.active = node
        node.location = space.cursor_location
        return node

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # needs active node editor and a tree to add nodes to
        return (space.type == 'NODE_EDITOR' and space.edit_tree)

    # Default invoke stores the mouse position to place the node correctly
    def invoke(self, context, event):
        self.store_mouse_cursor(context, event)
        return self.execute(context)


# Simple basic operator for adding a node
class NODE_OT_add_node(NodeAddOperator, Operator):
    '''Add a node to the active tree'''
    bl_idname = "node.add_node"
    bl_label = "Add Node"

    type = StringProperty(
            name="Node Type",
            description="Node type",
            )
    # optional group tree parameter for group nodes
    group_tree = StringProperty(
            name="Group tree",
            description="Group node tree name",
            )
    use_transform = BoolProperty(
            name="Use Transform",
            description="Start transform operator after inserting the node",
            default=False,
            )

    def execute(self, context):
        node = self.create_node(context, self.type)

        # set the node group tree of a group node
        if self.properties.is_property_set('group_tree'):
            node.node_tree = bpy.data.node_groups[self.group_tree]

        return {'FINISHED'}

    def invoke(self, context, event):
        self.store_mouse_cursor(context, event)
        result = self.execute(context)
        if self.use_transform and ('FINISHED' in result):
            return bpy.ops.transform.translate('INVOKE_DEFAULT')
        else:
            return result


def node_classes_iter(base=bpy.types.Node):
    """
    Yields all true node classes by checking for the is_registered_node_type classmethod.
    Node types can use specialized subtypes of bpy.types.Node, which are not usable
    nodes themselves (e.g. CompositorNode).
    """
    if base.is_registered_node_type():
        yield base
    for subclass in base.__subclasses__():
        for node_class in node_classes_iter(subclass):
            yield node_class


def node_class_items_iter(node_class, context):
    identifier = node_class.bl_rna.identifier
    # XXX Checking for explicit group node types is stupid.
    # This should be replaced by a generic system of generating
    # node items via callback.
    # Group node_tree pointer should also use a poll function to filter the library list,
    # but cannot do that without a node instance here. A node callback could just use the internal poll function.
    if identifier in {'ShaderNodeGroup', 'CompositorNodeGroup', 'TextureNodeGroup'}:
        tree_idname = context.space_data.edit_tree.bl_idname
        for group in bpy.data.node_groups:
            if group.bl_idname == tree_idname:
                yield (group.name, "", {"node_tree":group}) # XXX empty string should be replaced by description from tree
    else:
        yield (node_class.bl_rna.name, node_class.bl_rna.description, {})


def node_items_iter(context):
    snode = context.space_data
    if not snode:
        return
    tree = snode.edit_tree
    if not tree:
        return

    for node_class in node_classes_iter():
        if node_class.poll(tree):
            for item in node_class_items_iter(node_class, context):
                yield (node_class,) + item


# Create an enum list from node class items
def node_type_items_cb(self, context):
    return [(str(index), item[1], item[2]) for index, item in enumerate(node_items_iter(context))]


class NODE_OT_add_search(NodeAddOperator, Operator):
    '''Add a node to the active tree'''
    bl_idname = "node.add_search"
    bl_label = "Search and Add Node"
    bl_options = {'REGISTER', 'UNDO'}

    # XXX this should be called 'node_type' but the operator search
    # property is hardcoded to 'type' by a hack in bpy_operator_wrap.c ...
    type = EnumProperty(
            name="Node Type",
            description="Node type",
            items=node_type_items_cb,
            )

    def execute(self, context):
        for index, item in enumerate(node_items_iter(context)):
            if str(index) == self.type:
                node = self.create_node(context, item[0].bl_rna.identifier)
                for prop,value in item[3].items():
                    setattr(node, prop, value)
                break
        return {'FINISHED'}

    def invoke(self, context, event):
        self.store_mouse_cursor(context, event)
        # Delayed execution in the search popup
        context.window_manager.invoke_search_popup(self)
        return {'CANCELLED'}


# Simple basic operator for adding a node without further initialization
class NODE_OT_add_node(NodeAddOperator, bpy.types.Operator):
    '''Add a node to the active tree'''
    bl_idname = "node.add_node"
    bl_label = "Add Node"

    type = StringProperty(name="Node Type", description="Node type")

    def execute(self, context):
        node = self.create_node(context, self.type)
        return {'FINISHED'}


class NODE_OT_add_group_node(NodeAddOperator, bpy.types.Operator):
    '''Add a group node to the active tree'''
    bl_idname = "node.add_group_node"
    bl_label = "Add Group Node"

    type = StringProperty(name="Node Type", description="Node type")
    grouptree = StringProperty(name="Group tree", description="Group node tree name")

    def execute(self, context):
        node = self.create_node(context, self.type)
        node.node_tree = bpy.data.node_groups[self.grouptree]

        return {'FINISHED'}


class NODE_OT_collapse_hide_unused_toggle(Operator):
    '''Toggle collapsed nodes and hide unused sockets'''
    bl_idname = "node.collapse_hide_unused_toggle"
    bl_label = "Collapse and Hide Unused Sockets"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        space = context.space_data
        # needs active node editor and a tree
        return (space.type == 'NODE_EDITOR' and space.edit_tree)

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

