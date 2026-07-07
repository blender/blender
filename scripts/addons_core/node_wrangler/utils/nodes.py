# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy_extras.node_utils import connect_sockets
from math import hypot, inf
from bpy.app.translations import pgettext_tip as tip_


def force_update(context):
    context.space_data.node_tree.update_tag()


def dpi_fac():
    prefs = bpy.context.preferences.system
    return prefs.dpi / 72


def prefs_line_width():
    prefs = bpy.context.preferences.system
    return prefs.pixel_size


def node_mid_pt(node, axis):
    if axis == 'x':
        d = node.location.x + (node.dimensions.x / 2)
    elif axis == 'y':
        d = node.location.y - (node.dimensions.y / 2)
    else:
        d = 0
    return d


def autolink(node1, node2, links):
    available_inputs = [inp for inp in node2.inputs if inp.enabled and not inp.hide]
    available_outputs = [outp for outp in node1.outputs if outp.enabled and not outp.hide]
    for outp in available_outputs:
        for inp in available_inputs:
            if not inp.is_linked and inp.name == outp.name:
                connect_sockets(outp, inp)
                return True

    for outp in available_outputs:
        for inp in available_inputs:
            if not inp.is_linked and inp.type == outp.type:
                connect_sockets(outp, inp)
                return True

    # force some connection even if the type doesn't match
    if available_outputs:
        for inp in available_inputs:
            if not inp.is_linked:
                connect_sockets(available_outputs[0], inp)
                return True

    # even if no sockets are open, force one of matching type
    for outp in available_outputs:
        for inp in available_inputs:
            if inp.type == outp.type:
                connect_sockets(outp, inp)
                return True

    # do something!
    for outp in available_outputs:
        for inp in available_inputs:
            connect_sockets(outp, inp)
            return True

    print("Could not make a link from " + node1.name + " to " + node2.name)
    return False


def abs_node_location(node):
    abs_location = node.location
    if node.parent is None:
        return abs_location
    return abs_location + abs_node_location(node.parent)


def node_at_pos(nodes, context, event):
    nodes_under_mouse = []
    target_node = None

    store_mouse_cursor(context, event)
    x, y = context.space_data.cursor_location

    # Make a list of each corner (and middle of border) for each node.
    # Will be sorted to find nearest point and thus nearest node
    node_points_with_dist = []
    for node in nodes:
        skipnode = False
        if node.type != 'FRAME':  # no point trying to link to a frame node
            dimx = node.dimensions.x / dpi_fac()
            dimy = node.dimensions.y / dpi_fac()
            locx, locy = abs_node_location(node)

            if not skipnode:
                node_points_with_dist.append([node, hypot(x - locx, y - locy)])  # Top Left
                node_points_with_dist.append([node, hypot(x - (locx + dimx), y - locy)])  # Top Right
                node_points_with_dist.append([node, hypot(x - locx, y - (locy - dimy))])  # Bottom Left
                node_points_with_dist.append([node, hypot(x - (locx + dimx), y - (locy - dimy))])  # Bottom Right

                node_points_with_dist.append([node, hypot(x - (locx + (dimx / 2)), y - locy)])  # Mid Top
                node_points_with_dist.append([node, hypot(x - (locx + (dimx / 2)), y - (locy - dimy))])  # Mid Bottom
                node_points_with_dist.append([node, hypot(x - locx, y - (locy - (dimy / 2)))])  # Mid Left
                node_points_with_dist.append([node, hypot(x - (locx + dimx), y - (locy - (dimy / 2)))])  # Mid Right

    nearest_node = sorted(node_points_with_dist, key=lambda k: k[1])[0][0]

    for node in nodes:
        if node.type != 'FRAME' and skipnode == False:
            locx, locy = abs_node_location(node)
            dimx = node.dimensions.x / dpi_fac()
            dimy = node.dimensions.y / dpi_fac()
            if (locx <= x <= locx + dimx) and \
               (locy - dimy <= y <= locy):
                nodes_under_mouse.append(node)

    if len(nodes_under_mouse) == 1:
        if nodes_under_mouse[0] != nearest_node:
            target_node = nodes_under_mouse[0]  # use the node under the mouse if there is one and only one
        else:
            target_node = nearest_node  # else use the nearest node
    else:
        target_node = nearest_node
    return target_node


def store_mouse_cursor(context, event):
    space = context.space_data
    v2d = context.region.view2d
    tree = space.edit_tree

    # convert mouse position to the View2D for later node placement
    if context.region.type == 'WINDOW':
        space.cursor_location_from_region(event.mouse_region_x, event.mouse_region_y)
    else:
        space.cursor_location = tree.view_center


def get_nodes_links(context):
    tree = context.space_data.edit_tree
    return tree.nodes, tree.links


def get_internal_socket(socket):
    # get the internal socket from a socket inside or outside the group
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


def get_group_output_node(tree, output_node_type='GROUP_OUTPUT'):
    for node in tree.nodes:
        if node.type == output_node_type and node.is_active_output:
            return node


def get_output_location(tree):
    # get right-most location
    sorted_by_xloc = (sorted(tree.nodes, key=lambda x: x.location.x))
    max_xloc_node = sorted_by_xloc[-1]

    # get average y location
    sum_yloc = 0
    for node in tree.nodes:
        sum_yloc += node.location.y

    loc_x = max_xloc_node.location.x + max_xloc_node.dimensions.x + 80
    loc_y = sum_yloc / len(tree.nodes)
    return loc_x, loc_y


def get_viewer_image():
    for img in bpy.data.images:
        if (img.source == 'VIEWER'
                and len(img.render_slots) == 0
                and sum(img.size) > 0):
            return img


def nw_check(cls, context):
    space = context.space_data
    if space.type != 'NODE_EDITOR':
        cls.poll_message_set("Current editor is not a node editor.")
        return False
    if space.node_tree is None:
        cls.poll_message_set("No node tree was found in the current node editor.")
        return False
    if space.node_tree.library is not None:
        cls.poll_message_set("Current node tree is linked from another .blend file.")
        return False
    return True


def nw_check_not_empty(cls, context):
    if not context.space_data.edit_tree.nodes:
        cls.poll_message_set("Current node tree does not contain any nodes.")
        return False
    return True


def nw_check_active(cls, context):
    if context.active_node is None or not context.active_node.select:
        cls.poll_message_set("No active node.")
        return False
    return True


def nw_check_selected(cls, context, min=1, max=inf):
    num_selected = len(context.selected_nodes)
    if num_selected < min:
        if min > 1:
            poll_message = tip_("At least {:d} nodes must be selected.").format(min)
        else:
            poll_message = tip_("At least one node must be selected.")
        cls.poll_message_set(poll_message)
        return False
    if num_selected > max:
        poll_message = tip_("{:d} nodes are selected, but this operator can only work on {:d}.").format(
            num_selected, max)
        cls.poll_message_set(poll_message)
        return False
    return True


def nw_check_space_type(cls, context, types):
    if context.space_data.tree_type not in types:
        tree_types_str = ", ".join(t.split('NodeTree')[0].lower() for t in sorted(types))
        poll_message = tip_("Current node tree type not supported.\n"
                            "Should be one of {:s}.").format(tree_types_str)
        cls.poll_message_set(poll_message)
        return False
    return True


def nw_check_node_type(cls, context, type, invert=False):
    if invert and context.active_node.type == type:
        poll_message = tip_("Active node should not be of type {:s}.").format(type)
        cls.poll_message_set(poll_message)
        return False
    elif not invert and context.active_node.type != type:
        poll_message = tip_("Active node should be of type {:s}.").format(type)
        cls.poll_message_set(poll_message)
        return False
    return True


def nw_check_visible_outputs(cls, context):
    if not any(is_visible_socket(out) for out in context.active_node.outputs):
        cls.poll_message_set("Current node has no visible outputs.")
        return False
    return True


def nw_check_viewer_node(cls):
    if get_viewer_image() is None:
        cls.poll_message_set("Viewer image not found.")
        return False
    return True


def get_first_enabled_output(node):
    for output in node.outputs:
        if output.enabled:
            return output
    else:
        return node.outputs[0]


def is_visible_socket(socket):
    return not socket.hide and socket.enabled and socket.type != 'CUSTOM'


class NWBase:
    @classmethod
    def poll(cls, context):
        return nw_check(cls, context)


class NWBaseMenu:
    @classmethod
    def poll(cls, context):
        space = context.space_data
        return (space.type == 'NODE_EDITOR'
                and space.node_tree is not None
                and space.node_tree.library is None)
