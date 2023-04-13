# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "find_node_input",
)


def connect_sockets(input, output):
    """
    Connect sockets in a node tree.

    This is useful because the links created through the normal Python API are
    invalid when one of the sockets is a virtual socket (grayed out sockets in
    Group Input and Group Output nodes).

    It replaces node_tree.links.new(input, output)
    """
    import bpy

    # Swap sockets if they are not passed in the proper order
    if input.is_output and not output.is_output:
        input, output = output, input

    input_node = output.node
    output_node = input.node

    if input_node.id_data is not output_node.id_data:
        print("Sockets do not belong to the same node tree")
        return

    if type(input) == type(output) == bpy.types.NodeSocketVirtual:
        print("Cannot connect two virtual sockets together")
        return

    if output_node.type == 'GROUP_OUTPUT' and type(input) == bpy.types.NodeSocketVirtual:
        output_node.id_data.outputs.new(type(output).__name__, output.name)
        input = output_node.inputs[-2]

    if input_node.type == 'GROUP_INPUT' and type(output) == bpy.types.NodeSocketVirtual:
        output_node.id_data.inputs.new(type(input).__name__, input.name)
        output = input_node.outputs[-2]

    return input_node.id_data.links.new(input, output)


# XXX Names are not unique. Returns the first match.
def find_node_input(node, name):
    for input in node.inputs:
        if input.name == name:
            return input

    return None
