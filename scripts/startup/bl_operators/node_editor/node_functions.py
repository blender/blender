# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.app.translations import pgettext_tip as tip_


def node_editor_poll(cls, context):
    space = context.space_data
    if space is None or (space.type != 'NODE_EDITOR'):
        cls.poll_message_set("Active editor is not a node editor.")
        return False
    if space.node_tree is None:
        cls.poll_message_set("Node tree was not found in the active node editor.")
        return False
    if space.node_tree.library is not None:
        cls.poll_message_set("Active node tree is linked from another .blend file.")
        return False
    if not space.edit_tree.nodes:
        cls.poll_message_set("Active node tree does not contain any nodes.")
        return False
    return True


def node_space_type_poll(cls, context, types):
    if context.space_data.tree_type not in types:
        tree_types_str = ", ".join(t.split("NodeTree")[0].lower() for t in sorted(types))
        poll_message = tip_(
            "Current node tree type not supported.\n"
            "Should be one of {:s}."
        ).format(tree_types_str)
        cls.poll_message_set(poll_message)
        return False
    return True


def get_group_output_node(tree, output_node_idname='NodeGroupOutput'):
    for node in tree.nodes:
        if node.bl_idname == output_node_idname and node.is_active_output:
            return node


def get_output_location(tree):
    # get right-most location.
    sorted_by_xloc = (sorted(tree.nodes, key=lambda x: x.location.x))
    max_xloc_node = sorted_by_xloc[-1]

    # get average y location.
    sum_yloc = 0
    for node in tree.nodes:
        sum_yloc += node.location.y

    loc_x = max_xloc_node.location.x + max_xloc_node.dimensions.x + 80
    loc_y = sum_yloc / len(tree.nodes)
    return loc_x, loc_y


def get_internal_socket(socket):
    # get the internal socket from a socket inside or outside the group.
    node = socket.node
    if node.type == 'GROUP_OUTPUT':
        iterator = node.id_data.interface.items_tree
    elif node.type == 'GROUP_INPUT':
        iterator = node.id_data.interface.items_tree
    elif hasattr(node, "node_tree"):
        iterator = node.node_tree.interface.items_tree
    else:
        return None

    for s in iterator:
        if s.identifier == socket.identifier:
            return s
    return iterator[0]


def is_visible_socket(socket):
    return socket.is_icon_visible and socket.type != 'CUSTOM'


def is_viewer_link(link, output_node):
    if link.to_node == output_node and link.to_socket == output_node.inputs[0]:
        return True
    if link.to_node.type == 'GROUP_OUTPUT':
        socket = get_internal_socket(link.to_socket)
        if socket.is_inspect_output:
            return True
    return False


def force_update(context):
    context.space_data.node_tree.update_tag()


class NodeEditorBase:
    @classmethod
    def poll(cls, context):
        return node_editor_poll(cls, context)
