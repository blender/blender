# SPDX-FileCopyrightText: 2013-2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import BoolProperty
from bpy_extras.node_utils import connect_sockets

from .node_editor.node_functions import (
    NodeEditorBase,
    node_editor_poll,
    node_space_type_poll,
    get_group_output_node,
    get_output_location,
    get_internal_socket,
    is_visible_socket,
    is_viewer_socket,
    is_viewer_link,
    viewer_socket_name,
    force_update,
)


class NODE_OT_connect_to_output(Operator, NodeEditorBase):
    bl_idname = "node.connect_to_output"
    bl_label = "Connect to Output"
    bl_description = "Connect active node to the active output node of the node tree"
    bl_options = {'REGISTER', 'UNDO'}

    # If false, the operator is not executed if the current node group happens to be a geometry nodes group.
    # This is needed because geometry nodes has its own viewer node that uses the same shortcut as in the compositor.
    run_in_geometry_nodes: BoolProperty(
        name="Run in Geometry Nodes Editor",
        default=True,
    )

    def __init__(self):
        self.shader_output_idname = ""

    @classmethod
    def poll(cls, context):
        """Already implemented natively for compositing nodes."""
        return (node_editor_poll(cls, context) and
                node_space_type_poll(cls, context, {'ShaderNodeTree', 'GeometryNodeTree'}))

    @staticmethod
    def get_output_sockets(node_tree):
        return [item for item in node_tree.interface.items_tree
                if item.item_type == 'SOCKET' and item.in_out == 'OUTPUT']

    def init_shader_variables(self, space, shader_type):
        """Get correct output node in shader editor"""
        if shader_type == 'OBJECT':
            if space.id in bpy.data.lights.values():
                self.shader_output_idname = 'ShaderNodeOutputLight'
            else:
                self.shader_output_idname = 'ShaderNodeOutputMaterial'
        elif shader_type == 'WORLD':
            self.shader_output_idname = 'ShaderNodeOutputWorld'

    def ensure_viewer_socket(self, node_tree, socket_type, connect_socket=None):
        """Check if a viewer output already exists in a node group, otherwise create it"""
        viewer_socket = None
        output_sockets = self.get_output_sockets(node_tree)
        if len(output_sockets):
            for i, socket in enumerate(output_sockets):
                if is_viewer_socket(socket) and socket.socket_type == socket_type:
                    # If viewer output is already used but leads to the same socket we can still use it.
                    is_used = self.has_socket_other_users(socket)
                    if is_used:
                        if connect_socket is None:
                            continue
                        groupout = get_group_output_node(node_tree)
                        groupout_input = groupout.inputs[i]
                        links = groupout_input.links
                        if connect_socket not in [link.from_socket for link in links]:
                            continue
                        viewer_socket = socket
                        break

        if viewer_socket is None:
            # Create viewer socket.
            viewer_socket = node_tree.interface.new_socket(
                viewer_socket_name, in_out='OUTPUT', socket_type=socket_type)
            viewer_socket.is_inspect_output = True
        return viewer_socket

    @staticmethod
    def ensure_group_output(node_tree):
        """Check if a group output node exists, otherwise create it"""
        groupout = get_group_output_node(node_tree)
        if groupout is None:
            groupout = node_tree.nodes.new('NodeGroupOutput')
            loc_x, loc_y = get_output_location(node_tree)
            groupout.location.x = loc_x
            groupout.location.y = loc_y
            groupout.select = False
            # So that we don't keep on adding new group outputs.
            groupout.is_active_output = True
        return groupout

    @classmethod
    def search_sockets(cls, node, r_sockets, index=None):
        """Recursively scan nodes for viewer sockets and store them in a list"""
        for i, input_socket in enumerate(node.inputs):
            if index and i != index:
                continue
            if len(input_socket.links):
                link = input_socket.links[0]
                next_node = link.from_node
                external_socket = link.from_socket
                if hasattr(next_node, "node_tree"):
                    for socket_index, socket in enumerate(next_node.node_tree.interface.items_tree):
                        if socket.identifier == external_socket.identifier:
                            break
                    if is_viewer_socket(socket) and socket not in r_sockets:
                        r_sockets.append(socket)
                        # continue search inside of node group but restrict socket to where we came from.
                        groupout = get_group_output_node(next_node.node_tree)
                        cls.search_sockets(groupout, r_sockets, index=socket_index)

    @classmethod
    def scan_nodes(cls, tree, sockets):
        """Recursively get all viewer sockets in a material tree"""
        for node in tree.nodes:
            if hasattr(node, "node_tree"):
                if node.node_tree is None:
                    continue
                for socket in cls.get_output_sockets(node.node_tree):
                    if is_viewer_socket(socket) and (socket not in sockets):
                        sockets.append(socket)
                cls.scan_nodes(node.node_tree, sockets)

    @staticmethod
    def remove_socket(tree, socket):
        interface = tree.interface
        interface.remove(socket)
        interface.active_index = min(interface.active_index, len(interface.items_tree) - 1)

    def link_leads_to_used_socket(self, link):
        """Return True if link leads to a socket that is already used in this node"""
        socket = get_internal_socket(link.to_socket)
        return socket and self.is_socket_used_active_tree(socket)

    def is_socket_used_active_tree(self, socket):
        """Ensure used sockets in active node tree is calculated and check given socket"""
        if not hasattr(self, "used_viewer_sockets_active_mat"):
            self.used_viewer_sockets_active_mat = []

            node_tree = bpy.context.space_data.node_tree
            output_node = None
            if node_tree.type == 'GEOMETRY':
                output_node = get_group_output_node(node_tree)
            elif node_tree.type == 'SHADER':
                output_node = get_group_output_node(node_tree, output_node_idname=self.shader_output_idname)

            if output_node is not None:
                self.search_sockets(output_node, self.used_viewer_sockets_active_mat)
        return socket in self.used_viewer_sockets_active_mat

    def has_socket_other_users(self, socket):
        """List the other users for this socket (other materials or geometry nodes groups)"""
        if not hasattr(self, "other_viewer_sockets_users"):
            self.other_viewer_sockets_users = []
            if socket.socket_type == 'NodeSocketShader':
                for mat in bpy.data.materials:
                    if mat.node_tree == bpy.context.space_data.node_tree or not hasattr(mat.node_tree, "nodes"):
                        continue
                    # Get viewer node.
                    output_node = get_group_output_node(mat.node_tree,
                                                        output_node_idname=self.shader_output_idname)
                    if output_node is not None:
                        self.search_sockets(output_node, self.other_viewer_sockets_users)
            elif socket.socket_type == 'NodeSocketGeometry':
                for obj in bpy.data.objects:
                    for mod in obj.modifiers:
                        if mod.type != 'NODES' or mod.node_group == bpy.context.space_data.node_tree:
                            continue
                        # Get viewer node.
                        output_node = get_group_output_node(mod.node_group)
                        if output_node is not None:
                            self.search_sockets(output_node, self.other_viewer_sockets_users)
        return socket in self.other_viewer_sockets_users

    def get_output_index(self, node, output_node, is_base_node_tree, socket_type, check_type=False):
        """Get the next available output socket in the active node"""
        out_i = None
        valid_outputs = []
        for i, out in enumerate(node.outputs):
            if is_visible_socket(out) and (not check_type or out.type == socket_type):
                valid_outputs.append(i)
        if valid_outputs:
            out_i = valid_outputs[0]  # Start index of node's outputs.
        for i, valid_i in enumerate(valid_outputs):
            for out_link in node.outputs[valid_i].links:
                if is_viewer_link(out_link, output_node):
                    if is_base_node_tree or self.link_leads_to_used_socket(out_link):
                        if i < len(valid_outputs) - 1:
                            out_i = valid_outputs[i + 1]
                        else:
                            out_i = valid_outputs[0]
        return out_i

    def create_links(self, path, node, active_node_socket_id, socket_type):
        """Create links at each step in the node group path."""
        path = list(reversed(path))
        # Starting from the level of the active node.
        for path_index, path_element in enumerate(path[:-1]):
            # Ensure there is a viewer node and it has an input.
            tree = path_element.node_tree
            viewer_socket = self.ensure_viewer_socket(
                tree, socket_type,
                connect_socket=node.outputs[active_node_socket_id]
                if path_index == 0 else None)
            if viewer_socket in self.delete_sockets:
                self.delete_sockets.remove(viewer_socket)

            # Connect the current to its viewer.
            link_start = node.outputs[active_node_socket_id]
            link_end = self.ensure_group_output(tree).inputs[viewer_socket.identifier]
            connect_sockets(link_start, link_end)

            # Go up in the node group hierarchy.
            next_tree = path[path_index + 1].node_tree
            node = next(n for n in next_tree.nodes
                        if n.type == 'GROUP'
                        and n.node_tree == tree)
            tree = next_tree
            active_node_socket_id = viewer_socket.identifier
        return node.outputs[active_node_socket_id]

    def cleanup(self):
        # Delete sockets.
        for socket in self.delete_sockets:
            if not self.has_socket_other_users(socket):
                tree = socket.id_data
                self.remove_socket(tree, socket)

    def invoke(self, context, event):
        space = context.space_data
        # Ignore operator when running in wrong context.
        if self.run_in_geometry_nodes != (space.tree_type == 'GeometryNodeTree'):
            return {'PASS_THROUGH'}

        mlocx = event.mouse_region_x
        mlocy = event.mouse_region_y
        select_node = bpy.ops.node.select(location=(mlocx, mlocy), extend=False)
        if 'FINISHED' not in select_node:  # only run if mouse click is on a node.
            return {'CANCELLED'}

        base_node_tree = space.node_tree
        active_tree = context.space_data.edit_tree
        path = context.space_data.path
        nodes = active_tree.nodes
        active = nodes.active

        if not active and not any(is_visible_socket(out) for out in active.outputs):
            return {'CANCELLED'}

        # Scan through all nodes in tree including nodes inside of groups to find viewer sockets.
        self.delete_sockets = []
        self.scan_nodes(base_node_tree, self.delete_sockets)

        if not active.outputs:
            self.cleanup()
            return {'CANCELLED'}

        # For geometry node trees, we just connect to the group output.
        if space.tree_type == 'GeometryNodeTree':
            socket_type = 'NodeSocketGeometry'

            # Find (or create if needed) the output of this node tree.
            output_node = self.ensure_group_output(base_node_tree)

            active_node_socket_index = self.get_output_index(
                active, output_node, base_node_tree == active_tree, 'GEOMETRY', check_type=True
            )
            # If there is no 'GEOMETRY' output type - We can't preview the node.
            if active_node_socket_index is None:
                return {'CANCELLED'}

            # Find an input socket of the output of type geometry.
            output_node_socket_index = None
            for i, inp in enumerate(output_node.inputs):
                if inp.type == 'GEOMETRY':
                    output_node_socket_index = i
                    break
            if output_node_socket_index is None:
                output_node_socket_index = self.ensure_viewer_socket(
                    base_node_tree, socket_type, connect_socket=None)

        # For shader node trees, we connect to a material output.
        elif space.tree_type == 'ShaderNodeTree':
            socket_type = 'NodeSocketShader'
            self.init_shader_variables(space, space.shader_type)

            # Get or create material_output node.
            output_node = get_group_output_node(base_node_tree,
                                                output_node_idname=self.shader_output_idname)
            if not output_node:
                output_node = base_node_tree.nodes.new(self.shader_output_idname)
                output_node.location = get_output_location(base_node_tree)
                output_node.select = False

            active_node_socket_index = self.get_output_index(
                active, output_node, base_node_tree == active_tree, 'SHADER'
            )

            # Cancel if no socket was found. This can happen for group input
            # nodes with only a virtual socket output.
            if active_node_socket_index is None:
                return {'CANCELLED'}

            if active.outputs[active_node_socket_index].name == "Volume":
                output_node_socket_index = 1
            else:
                output_node_socket_index = 0

        # If there are no nested node groups, the link starts at the active node.
        node_output = active.outputs[active_node_socket_index]
        if len(path) > 1:
            # Recursively connect inside nested node groups and get the one from base level.
            node_output = self.create_links(path, active, active_node_socket_index, socket_type)
        output_node_input = output_node.inputs[output_node_socket_index]

        # Connect at base level.
        connect_sockets(node_output, output_node_input)

        self.cleanup()
        nodes.active = active
        active.select = True
        force_update(context)
        return {'FINISHED'}


classes = (
    NODE_OT_connect_to_output,
)
